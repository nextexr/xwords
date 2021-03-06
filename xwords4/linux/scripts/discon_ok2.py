#!/usr/bin/env python3

import re, os, sys, getopt, shutil, threading, requests, json, glob
import argparse, datetime, random, signal, subprocess, time
from shutil import rmtree

# LOGDIR=./$(basename $0)_logs
# APP_NEW=""
# DO_CLEAN=""
# APP_NEW_PARAMS=""
# NGAMES = 1
g_UDP_PCT_START = 100
gDeadLaunches = 0
# UDP_PCT_INCR=10
# UPGRADE_ODDS=""
# NROOMS=""
# HOST=""
# PORT=""
# TIMEOUT=""
# SAVE_GOOD=""
# MINDEVS=""
# MAXDEVS=""
# ONEPER=""
# RESIGN_PCT=0
g_DROP_N=0
# MINRUN=2		                # seconds
# ONE_PER_ROOM=""                 # don't run more than one device at a time per room
# USE_GTK=""
# UNDO_PCT=0
# ALL_VIA_RQ=${ALL_VIA_RQ:-FALSE}
# SEED=""
# BOARD_SIZES_OLD=(15)
# BOARD_SIZES_NEW=(15)
g_NAMES = [None, 'Brynn', 'Ariela', 'Kati', 'Eric']
# SEND_CHAT=''
# CORE_COUNT=$(ls core.* 2>/dev/null | wc -l)
# DUP_PACKETS=''
# HTTP_PCT=0

# declare -A PIDS
# declare -A APPS
# declare -A NEW_ARGS
# declare -a ARGS
# declare -A ARGS_DEVID
# declare -A ROOMS
# declare -A FILES
# declare -A LOGS
# declare -A MINEND
# ROOM_PIDS = {}
# declare -a APPS_OLD=()
# declare -a DICTS=				# wants to be =() too?
# declare -A CHECKED_ROOMS

# function cleanup() {
#     APP="$(basename $APP_NEW)"
#     while pidof $APP; do
#         echo "killing existing $APP instances..."
#         killall -9 $APP
#         sleep 1
#     done
#     echo "cleaning everything up...."
#     if [ -d $LOGDIR ]; then
#         mv $LOGDIR /tmp/${LOGDIR}_$$
#     fi
#     if [ -e $(dirname $0)/../../relay/xwrelay.log ]; then
#         mkdir -p /tmp/${LOGDIR}_$$
#         mv $(dirname $0)/../../relay/xwrelay.log /tmp/${LOGDIR}_$$
#     fi

#     echo "DELETE FROM games WHERE room LIKE 'ROOM_%';" | psql -q -t xwgames
#     echo "DELETE FROM msgs WHERE NOT devid in (SELECT unnest(devids) from games);" | psql -q -t xwgames
# }

# function connName() {
#     LOG=$1
#     grep -a 'got_connect_cmd: connName' $LOG | \
#         tail -n 1 | \
#         sed 's,^.*connName: \"\(.*\)\" (reconnect=.)$,\1,'
# }

# function check_room() {
#     ROOM=$1
#     if [ -z ${CHECKED_ROOMS[$ROOM]:-""} ]; then
#         NUM=$(echo "SELECT COUNT(*) FROM games "\
#             "WHERE NOT dead "\
#             "AND ntotal!=sum_array(nperdevice) "\
#             "AND ntotal != -sum_array(nperdevice) "\
#             "AND room='$ROOM'" |
#             psql -q -t xwgames)
#         NUM=$((NUM+0))
#         if [ "$NUM" -gt 0 ]; then
#             echo "$ROOM in the DB has unconsummated games.  Remove them."
#             exit 1
#         else
#             CHECKED_ROOMS[$ROOM]=1
#         fi
#     fi
# }

# print_cmdline() {
#     local COUNTER=$1
#     local LOG=${LOGS[$COUNTER]}
#     echo -n "New cmdline: " >> $LOG
#     echo "${APPS[$COUNTER]} ${NEW_ARGS[$COUNTER]} ${ARGS[$COUNTER]}" >> $LOG
# }

def pick_ndevs(args):
    RNUM = random.randint(0, 99)
    if RNUM > 95 and args.MAXDEVS >= 4:
        NDEVS = 4
    elif RNUM > 90 and args.MAXDEVS >= 3:
        NDEVS = 3
    else:
        NDEVS = 2
    if NDEVS < args.MINDEVS:
        NDEVS = args.MINDEVS
    return NDEVS

# # Given a device count, figure out how many local players per device.
# # "1 1" would be a two-device game with 1 each.  "1 2 1" a
# # three-device game with four players total
def figure_locals(args, NDEVS):
    NPLAYERS = pick_ndevs(args)
    if NPLAYERS < NDEVS: NPLAYERS = NDEVS
    
    EXTRAS = 0
    if not args.ONEPER:
        EXTRAS = NPLAYERS - NDEVS

    LOCALS = []
    for IGNORE in range(NDEVS):
         COUNT = 1
         if EXTRAS > 0:
             EXTRA = random.randint(0, EXTRAS)
             if EXTRA > 0:
                 COUNT += EXTRA
                 EXTRAS -= EXTRA
         LOCALS.append(COUNT)
    assert 0 < sum(LOCALS) <= 4
    return LOCALS

def player_params(args, NLOCALS, NPLAYERS, NAME_INDX):
    assert 0 < NPLAYERS <= 4
    NREMOTES = NPLAYERS - NLOCALS
    PARAMS = []
    while NLOCALS > 0 or NREMOTES > 0:
        if 0 == random.randint(0, 2) and 0 < NLOCALS:
            PARAMS += ['--robot',  g_NAMES[NAME_INDX], '--robot-iq', str(random.randint(1,100))]
            NLOCALS -= 1
            NAME_INDX += 1
        elif 0 < NREMOTES:
            PARAMS += ['--remote-player']
            NREMOTES -= 1
    return PARAMS

def logReaderStub(dev): dev.logReaderMain()

class Device():
    sHasLDevIDMap = {}
    sConnNamePat = re.compile('.*got_connect_cmd: connName: "([^"]+)".*$')
    sGameOverPat = re.compile('.*\[unused tiles\].*')
    sTilesLeftPoolPat = re.compile('.*pool_removeTiles: (\d+) tiles left in pool')
    sTilesLeftTrayPat = re.compile('.*player \d+ now has (\d+) tiles')
    sRelayIDPat = re.compile('.*UPDATE games.*seed=(\d+),.*relayid=\'([^\']+)\'.*')
    
    def __init__(self, args, game, indx, params, room, peers, db, log, nInGame):
        self.game = game
        self.indx = indx
        self.args = args
        self.pid = 0
        self.gameOver = False
        self.params = params
        self.room = room
        self.db = db
        self.logPath = log
        self.nInGame = nInGame
        # runtime stuff; init now
        self.app = args.APP_OLD
        self.proc = None
        self.peers = peers
        self.devID = ''
        self.launchCount = 0
        self.allDone = False    # when true, can be killed
        self.nTilesLeftPool = None
        self.nTilesLeftTray = None
        self.relayID = None
        self.relaySeed = 0
        self.locked = False

        self.setApp(args.START_PCT)

        with open(self.logPath, "w") as log:
            log.write('New cmdline: ' + self.app + ' ' + (' '.join([str(p) for p in self.params])))
            log.write(os.linesep)

    def setApp(self, pct):
        if self.app == self.args.APP_OLD and not self.app == self.args.APP_NEW:
            if pct >= random.randint(0, 99):
                print('launch(): upgrading from ', self.app, ' to ', self.args.APP_NEW)
                self.app = self.args.APP_NEW

    def logReaderMain(self):
        assert self and self.proc
        stdout, stderr = self.proc.communicate()
        # print('logReaderMain called; opening:', self.logPath, 'flag:', flag)
        nLines = 0
        with open(self.logPath, 'a') as log:
            for line in stderr.splitlines():
                nLines += 1
                log.write(line + os.linesep)

                self.locked = True

                # check for game over
                if not self.gameOver:
                    match = Device.sGameOverPat.match(line)
                    if match: self.gameOver = True

                # Check every line for tiles left in pool
                match = Device.sTilesLeftPoolPat.match(line)
                if match: self.nTilesLeftPool = int(match.group(1))

                # Check every line for tiles left in tray
                match = Device.sTilesLeftTrayPat.match(line)
                if match: self.nTilesLeftTray = int(match.group(1))

                if not self.relayID:
                    match = Device.sRelayIDPat.match(line)
                    if match:
                        self.relaySeed = int(match.group(1))
                        self.relayID = match.group(2)

                self.locked = False

        # print('logReaderMain done, wrote lines:', nLines, 'to', self.logPath);

    def launch(self):
        args = []
        if self.args.VALGRIND:
            args += ['valgrind']
            # args += ['--leak-check=full']
            # args += ['--track-origins=yes']

        # Upgrade if appropriate
        self.setApp(self.args.UPGRADE_PCT)

        args += [self.app] + [str(p) for p in self.params]
        if self.devID: args.extend( ' '.split(self.devID))
        self.launchCount += 1
        self.proc = subprocess.Popen(args, stdout = subprocess.DEVNULL,
                                     stderr = subprocess.PIPE, universal_newlines = True)
        self.pid = self.proc.pid
        self.minEnd = datetime.datetime.now() + datetime.timedelta(seconds = self.args.MINRUN)

        # Now start a thread to read stdio
        self.reader = threading.Thread(target = logReaderStub, args=(self,))
        self.reader.isDaemon = True
        self.reader.start()

    def running(self):
        return self.proc and not self.proc.poll()

    def minTimeExpired(self):
        assert self.proc
        return self.minEnd < datetime.datetime.now()
        
    def kill(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            self.proc.wait()
            assert self.proc.poll() is not None

            self.reader.join()
            self.reader = None
        else:
            print('NOT killing')
        self.proc = None
        self.check_game()

    def handleAllDone(self):
        global gDeadLaunches
        if self.allDone:
            self.moveFiles()
            self.send_dead()
            gDeadLaunches += self.launchCount
        return self.allDone

    def moveFiles(self):
        assert not self.running()
        shutil.move(self.logPath, self.args.LOGDIR + '/done')
        shutil.move(self.db, self.args.LOGDIR + '/done')

    def send_dead(self):
        if self.args.ADD_RELAY:
            JSON = json.dumps([{'relayID': self.relayID, 'seed': self.relaySeed}])
            url = 'http://%s/xw4/relay.py/kill' % (self.args.HOST)
            params = {'params' : JSON}
            try:
                req = requests.get(url, params = params) # failing
            except requests.exceptions.ConnectionError:
                print('got exception sending to', url, params, '; is relay.py running as apache module?')

    def getTilesCount(self):
        assert not self.locked
        return {'index': self.indx,
                'nTilesLeftPool': self.nTilesLeftPool,
                'nTilesLeftTray': self.nTilesLeftTray,
                'launchCount': self.launchCount,
                'game': self.game,
        }

    def update_ldevid(self):
        if not self.app in Device.sHasLDevIDMap:
            hasLDevID = False
            proc = subprocess.Popen([self.app, '--help'], stderr=subprocess.PIPE)
            # output, err, = proc.communicate()
            for line in proc.stderr.readlines():
                if b'--ldevid' in line:
                    hasLDevID = True
                    break
            # print('found --ldevid:', hasLDevID);
            Device.sHasLDevIDMap[self.app] = hasLDevID

        if Device.sHasLDevIDMap[self.app]:
            RNUM = random.randint(0, 99)
            if not self.devID:
                if RNUM < 30:
                    self.devID = '--ldevid LINUX_TEST_%.5d_' % (self.indx)
            elif RNUM < 10:
                self.devID += 'x'

    def check_game(self):
        if self.gameOver and not self.allDone:
            allDone = True
            for dev in self.peers:
                if dev == self: continue
                if not dev.gameOver:
                    allDone = False
                    break

            if allDone:
                for dev in self.peers:
                    assert self.game == dev.game
                    dev.allDone = True

            # print('Closing', self.connname, datetime.datetime.now())
            # for dev in Device.sConnnameMap[self.connname]:
            #     dev.kill()
#         # kill_from_logs $OTHERS $KEY
#         for ID in $OTHERS $KEY; do
#             echo -n "${ID}:${LOGS[$ID]}, "
#             kill_from_log ${LOGS[$ID]} || /bin/true
# 			send_dead $ID
#             close_device $ID $DONEDIR "game over"
#         done
#         echo ""
#         # XWRELAY_ERROR_DELETED may be old
#     elif grep -aq 'relay_error_curses(XWRELAY_ERROR_DELETED)' $LOG; then
#         echo "deleting $LOG $(connName $LOG) b/c another resigned"
#         kill_from_log $LOG || /bin/true
#         close_device $KEY $DEADDIR "other resigned"
#     elif grep -aq 'relay_error_curses(XWRELAY_ERROR_DEADGAME)' $LOG; then
#         echo "deleting $LOG $(connName $LOG) b/c another resigned"
#         kill_from_log $LOG || /bin/true
#         close_device $KEY $DEADDIR "other resigned"
#     else
#         maybe_resign $KEY
#     fi
# }


def build_cmds(args):
    devs = []
    COUNTER = 0
    PLAT_PARMS = []
    if not args.USE_GTK:
        PLAT_PARMS += ['--curses', '--close-stdin']

    for GAME in range(1, args.NGAMES + 1):
        peers = set()
        ROOM = 'ROOM_%.3d' % (GAME % args.NROOMS)
        PHONE_BASE = '%.4d' % (GAME % args.NROOMS)
        NDEVS = pick_ndevs(args)
        LOCALS = figure_locals(args, NDEVS) # as array
        NPLAYERS = sum(LOCALS)
        assert(len(LOCALS) == NDEVS)
        DICT = args.DICTS[GAME % len(args.DICTS)]
        # make one in three games public
        PUBLIC = []
        if random.randint(0, 3) == 0: PUBLIC = ['--make-public', '--join-public']
        DEV = 0
        for NLOCALS in LOCALS:
            DEV += 1
            DB = '{}/{:02d}_{:02d}_DB.sql3'.format(args.LOGDIR, GAME, DEV)
            LOG = '{}/{:02d}_{:02d}_LOG.txt'.format(args.LOGDIR, GAME, DEV)

            PARAMS = player_params(args, NLOCALS, NPLAYERS, DEV)
            PARAMS += PLAT_PARMS
            PARAMS += ['--board-size', '15', '--trade-pct', args.TRADE_PCT, '--sort-tiles']

            # We SHOULD support having both SMS and relay working...
            if args.ADD_RELAY:
                PARAMS += [ '--relay-port', args.PORT, '--room', ROOM, '--host', args.HOST]
                if random.randint(0,100) % 100 < g_UDP_PCT_START:
                    PARAMS += ['--use-udp']
            if args.ADD_SMS:
                PARAMS += [ '--sms-number', PHONE_BASE + str(DEV - 1) ]
                if args.SMS_FAIL_PCT > 0:
                    PARAMS += [ '--sms-fail-pct', args.SMS_FAIL_PCT ]
                if DEV > 1:
                    PARAMS += [ '--server-sms-number', PHONE_BASE + '0' ]

            if args.UNDO_PCT > 0:
                PARAMS += ['--undo-pct', args.UNDO_PCT]
            PARAMS += [ '--game-dict', DICT]
            PARAMS += ['--slow-robot', '1:3', '--skip-confirm']
            PARAMS += ['--db', DB]

            PARAMS += ['--drop-nth-packet', g_DROP_N]
            if random.randint(0, 100) < args.HTTP_PCT:
                PARAMS += ['--use-http']

            PARAMS += ['--split-packets', '2']
            if args.SEND_CHAT:
                PARAMS += ['--send-chat', args.SEND_CHAT]

            if args.DUP_PACKETS:
                PARAMS += ['--dup-packets']
            # PARAMS += ['--my-port', '1024']
            # PARAMS += ['--savefail-pct', 10]

            # With the --seed param passed, games with more than 2
            # devices don't get going. No idea why. This param is NOT
            # passed in the old bash version of this script, so fixing
            # it isn't a priority.
            # PARAMS += ['--seed', args.SEED]
            PARAMS += PUBLIC
            if DEV > 1:
                PARAMS += ['--force-channel', DEV - 1]
            else:
                PARAMS += ['--server']

            # print('PARAMS:', PARAMS)

            dev = Device(args, GAME, COUNTER, PARAMS, ROOM, peers, DB, LOG, len(LOCALS))
            peers.add(dev)
            dev.update_ldevid()
            devs.append(dev)

            COUNTER += 1
    return devs

def summarizeTileCounts(devs, endTime, state):
    global gDeadLaunches
    shouldGoOn = True
    data = [dev.getTilesCount() for dev in devs]
    nDevs = len(data)
    totalTiles = 0
    colWidth = max(2, len(str(nDevs)))
    headWidth = 0
    fmtData = [{'head' : 'dev', },
               {'head' : 'launches', },
               {'head' : 'tls left', },
    ]
    for datum in fmtData:
        headWidth = max(headWidth, len(datum['head']))
        datum['data'] = []

    # Group devices by game
    games = []
    prev = -1
    for datum in data:
        gameNo = datum['game']
        if gameNo != prev:
            games.append([])
            prev = gameNo
        games[-1].append('{:0{width}d}'.format(datum['index'], width=colWidth))
    fmtData[0]['data'] = ['+'.join(game) for game in games]

    nLaunches = gDeadLaunches
    for datum in data:
        launchCount = datum['launchCount']
        nLaunches += launchCount
        fmtData[1]['data'].append('{:{width}d}'.format(launchCount, width=colWidth))

        # Format tiles left. It's the number in the bag/pool until
        # that drops to 0, then the number in the tray preceeded by
        # '+'. Only the pool number is included in the totalTiles sum.
        nTilesPool = datum['nTilesLeftPool']
        nTilesTray = datum['nTilesLeftTray']
        if nTilesPool is None and nTilesTray is None:
            txt = ('-' * colWidth)
        elif int(nTilesPool) == 0 and not nTilesTray is None:
            txt = '{:+{width}d}'.format(nTilesTray, width=colWidth-1)
        else:
            txt = '{:{width}d}'.format(nTilesPool, width=colWidth)
            totalTiles += int(nTilesPool)
        fmtData[2]['data'].append(txt)

    print('')
    print('devs left: {}; bag tiles left: {}; total launches: {}; {}/{}'
          .format(nDevs, totalTiles, nLaunches, datetime.datetime.now(), endTime ))
    fmt = '{head:>%d} {data}' % headWidth
    for datum in fmtData: datum['data'] = ' '.join(datum['data'])
    for datum in fmtData:
        print(fmt.format(**datum))

    # Now let's see if things are stuck: if the tile string hasn't
    # changed in two minutes bail. Note that the count of tiles left
    # isn't enough because it's zero for a long time as devices are
    # using up what's left in their trays and getting killed.
    now = datetime.datetime.now()
    tilesStr = fmtData[2]['data']
    if not 'tilesStr' in state or state['tilesStr'] != tilesStr:
        state['lastChange'] = now
        state['tilesStr'] = tilesStr

    return now - state['lastChange'] < datetime.timedelta(minutes = 1)

def countCores():
    return len(glob.glob1('/tmp',"core*"))

gDone = False

def run_cmds(args, devs):
    nCores = countCores()
    endTime = datetime.datetime.now() + datetime.timedelta(minutes = args.TIMEOUT_MINS)
    printState = {}
    lastPrint = datetime.datetime.now()

    while len(devs) > 0 and not gDone:
        if countCores() > nCores:
            print('core file count increased; exiting')
            break
        now = datetime.datetime.now()
        if now > endTime:
            print('outta time; outta here')
            break

        # print stats every 5 seconds
        if now - lastPrint > datetime.timedelta(seconds = 5):
            lastPrint = now
            if not summarizeTileCounts(devs, endTime, printState):
                print('no change in too long; exiting')
                break

        dev = random.choice(devs)
        if not dev.running():
            if dev.handleAllDone():
                devs.remove(dev)
            else:
#             if [ -n "$ONE_PER_ROOM" -a 0 -ne ${ROOM_PIDS[$ROOM]} ]; then
#                 continue
#             fi
#             try_upgrade $KEY
#             try_upgrade_upd $KEY
                dev.launch()
#             PID=$!
#             # renice doesn't work on one of my machines...
#             renice -n 1 -p $PID >/dev/null 2>&1 || /bin/true
#             PIDS[$KEY]=$PID
#             ROOM_PIDS[$ROOM]=$PID
#             MINEND[$KEY]=$(($NOW + $MINRUN))
        elif dev.minTimeExpired():
            dev.kill()
            if dev.handleAllDone():
                devs.remove(dev)
        else:
            time.sleep(1.0)

    # if we get here via a break, kill any remaining games
    if devs:
        print('stopping {} remaining games'.format(len(devs)))
        for dev in devs:
            if dev.running(): dev.kill()

# run_via_rq() {
#     # launch then kill all games to give chance to hook up
#     for KEY in ${!ARGS[*]}; do
#         echo "launching $KEY"
#         launch $KEY &
#         PID=$!
#         sleep 1
#         kill $PID
#         wait $PID
#         # add_pipe $KEY
#     done

#     echo "now running via rq"
#     # then run them
#     while :; do
#         COUNT=${#ARGS[*]}
#         [ 0 -ge $COUNT ] && break

#         INDX=$(($RANDOM%COUNT))
#         KEYS=( ${!ARGS[*]} )
#         KEY=${KEYS[$INDX]}
#         CMD=${ARGS[$KEY]}
            
#         RELAYID=$(./scripts/relayID.sh --short ${LOGS[$KEY]})
#         MSG_COUNT=$(../relay/rq -a $HOST -m $RELAYID 2>/dev/null | sed 's,^.*-- ,,')
#         if [ $MSG_COUNT -gt 0 ]; then
#             launch $KEY &
#             PID=$!
#             sleep 2
#             kill $PID || /bin/true
#             wait $PID
#         fi
#         [ "$DROP_N" -ge 0 ] && increment_drop $KEY
#         check_game $KEY
#     done
# } # run_via_rq

# function getArg() {
#     [ 1 -lt "$#" ] || usage "$1 requires an argument"
#     echo $2
# }

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--send-chat', dest = 'SEND_CHAT', type = str, default = None,
                        help = 'the message to send')

    parser.add_argument('--app-new', dest = 'APP_NEW', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll use')
    parser.add_argument('--app-old', dest = 'APP_OLD', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll upgrade from')
    parser.add_argument('--start-pct', dest = 'START_PCT', default = 50, type = int,
                        help = 'odds of starting with the new app, 0 <= n < 100')
    parser.add_argument('--upgrade-pct', dest = 'UPGRADE_PCT', default = 5, type = int,
                        help = 'odds of upgrading at any launch, 0 <= n < 100')

    parser.add_argument('--num-games', dest = 'NGAMES', type = int, default = 1, help = 'number of games')
    parser.add_argument('--num-rooms', dest = 'NROOMS', type = int, default = 0,
                        help = 'number of roooms (default to --num-games)')
    parser.add_argument('--timeout-mins', dest = 'TIMEOUT_MINS', default = 10000, type = int,
                        help = 'minutes after which to timeout')
    parser.add_argument('--log-root', dest='LOGROOT', default = '.', help = 'where logfiles go')
    parser.add_argument('--dup-packets', dest = 'DUP_PACKETS', default = False, help = 'send all packet twice')
    parser.add_argument('--use-gtk', dest = 'USE_GTK', default = False, action = 'store_true',
                        help = 'run games using gtk instead of ncurses')
    # # 
    # #     echo "    [--clean-start]                                         \\" >&2
    parser.add_argument('--game-dict', dest = 'DICTS', action = 'append', default = [])
    # #     echo "    [--help]                                                \\" >&2
    parser.add_argument('--host',  dest = 'HOST', default = 'localhost',
                        help = 'relay hostname')
    # #     echo "    [--max-devs <int>]                                      \\" >&2
    parser.add_argument('--min-devs', dest = 'MINDEVS', type = int, default = 2,
                        help = 'No game will have fewer devices than this')
    parser.add_argument('--max-devs', dest = 'MAXDEVS', type = int, default = 4,
                        help = 'No game will have more devices than this')

    parser.add_argument('--min-run', dest = 'MINRUN', type = int, default = 2,
                        help = 'Keep each run alive at least this many seconds')
    # #     echo "    [--new-app <path/to/app]                                \\" >&2
    # #     echo "    [--new-app-args [arg*]]  # passed only to new app       \\" >&2
    # #     echo "    [--num-rooms <int>]                                     \\" >&2
    # #     echo "    [--old-app <path/to/app]*                               \\" >&2
    parser.add_argument('--one-per', dest = 'ONEPER', default = False,
                        action = 'store_true', help = 'force one player per device')
    parser.add_argument('--port', dest = 'PORT', default = 10997, type = int, \
                        help = 'Port relay\'s on')
    parser.add_argument('--resign-pct', dest = 'RESIGN_PCT', default = 0, type = int, \
                        help = 'Odds of resigning [0..100]')
    parser.add_argument('--seed', type = int, dest = 'SEED',
                        default = random.randint(1, 1000000000))
    # #     echo "    [--send-chat <interval-in-seconds>                      \\" >&2
    # #     echo "    [--udp-incr <pct>]                                      \\" >&2
    # #     echo "    [--udp-start <pct>]      # default: $UDP_PCT_START                 \\" >&2
    # #     echo "    [--undo-pct <int>]                                      \\" >&2
    parser.add_argument('--http-pct', dest = 'HTTP_PCT', default = 0, type = int,
                        help = 'pct of games to be using web api')

    parser.add_argument('--undo-pct', dest = 'UNDO_PCT', default = 0, type = int)
    parser.add_argument('--trade-pct', dest = 'TRADE_PCT', default = 0, type = int)

    parser.add_argument('--add-sms', dest = 'ADD_SMS', default = False, action = 'store_true')
    parser.add_argument('--sms-fail-pct', dest = 'SMS_FAIL_PCT', default = 0, type = int)
    parser.add_argument('--remove-relay', dest = 'ADD_RELAY', default = True, action = 'store_false')

    parser.add_argument('--with-valgrind', dest = 'VALGRIND', default = False,
                        action = 'store_true')

    return parser

# #######################################################
# ##################### MAIN begins #####################
# #######################################################

def parseArgs():
    args = mkParser().parse_args()
    assignDefaults(args)
    print(args)
    return args
    # print(options)

# while [ "$#" -gt 0 ]; do
#     case $1 in
#         --udp-start)
#             UDP_PCT_START=$(getArg $*)
#             shift
#             ;;
#         --udp-incr)
#             UDP_PCT_INCR=$(getArg $*)
#             shift
#             ;;
#         --clean-start)
#             DO_CLEAN=1
#             ;;
#         --num-games)
#             NGAMES=$(getArg $*)
#             shift
#             ;;
#         --num-rooms)
#             NROOMS=$(getArg $*)
#             shift
#             ;;
#         --old-app)
#             APPS_OLD[${#APPS_OLD[@]}]=$(getArg $*)
#             shift
#             ;;
# 		--log-root)
# 			[ -d $2 ] || usage "$1: no such directory $2"
# 			LOGDIR=$2/$(basename $0)_logs
# 			shift
# 			;;
#         --dup-packets)
                  #             DUP_PACKETS=1
#             ;;
#         --new-app)
#             APP_NEW=$(getArg $*)
#             shift
#             ;;
#         --new-app-args)
#             APP_NEW_PARAMS="${2}"
#             echo "got $APP_NEW_PARAMS"
#             shift
#             ;;
#         --game-dict)
#             DICTS[${#DICTS[@]}]=$(getArg $*)
#             shift
#             ;;
#         --min-devs)
#             MINDEVS=$(getArg $*)
#             shift
#             ;;
#         --max-devs)
#             MAXDEVS=$(getArg $*)
#             shift
#             ;;
# 		--min-run)
# 			MINRUN=$(getArg $*)
# 			[ $MINRUN -ge 2 -a $MINRUN -le 60 ] || usage "$1: n must be 2 <= n <= 60"
# 			shift
# 			;;
#         --one-per)
#             ONEPER=TRUE
#             ;;
#         --host)
#             HOST=$(getArg $*)
#             shift
#             ;;
#         --port)
#             PORT=$(getArg $*)
#             shift
#             ;;
#         --seed)
#             SEED=$(getArg $*)
#             shift
#             ;;
#         --undo-pct)
#             UNDO_PCT=$(getArg $*)
#             shift
#             ;;
#         --http-pct)
#             HTTP_PCT=$(getArg $*)
#             [ $HTTP_PCT -ge 0 -a $HTTP_PCT -le 100 ] || usage "$1: n must be 0 <= n <= 100"
#             shift
#             ;;
#         --send-chat)
#             SEND_CHAT=$(getArg $*)
#             shift
#             ;;
#         --resign-pct)
#             RESIGN_PCT=$(getArg $*)
# 			[ $RESIGN_PCT -ge 0 -a $RESIGN_PCT -le 100 ] || usage "$1: n must be 0 <= n <= 100"
#             shift
#             ;;
# 		--no-timeout)
# 			TIMEOUT=0x7FFFFFFF
# 			;;
#         --help)
#             usage
#             ;;
#         *) usage "unrecognized option $1"
#             ;;
#     esac
#     shift
# done

def assignDefaults(args):
    if not args.NROOMS: args.NROOMS = args.NGAMES
    if len(args.DICTS) == 0: args.DICTS.append('CollegeEng_2to8.xwd')
    args.LOGDIR = os.path.basename(sys.argv[0]) + '_logs'
    # Move an existing logdir aside
    if os.path.exists(args.LOGDIR):
        shutil.move(args.LOGDIR, '/tmp/' + args.LOGDIR + '_' + str(random.randint(0, 100000)))
    for d in ['', 'done', 'dead',]:
        os.mkdir(args.LOGDIR + '/' + d)
# [ -z "$SAVE_GOOD" ] && SAVE_GOOD=YES
# # [ -z "$RESIGN_PCT" -a "$NGAMES" -gt 1 ] && RESIGN_RATIO=1000 || RESIGN_RATIO=0
# [ -z "$DROP_N" ] && DROP_N=0
# [ -z "$USE_GTK" ] && USE_GTK=FALSE
# [ -z "$UPGRADE_ODDS" ] && UPGRADE_ODDS=10
# #$((NGAMES/50))
# [ 0 -eq $UPGRADE_ODDS ] && UPGRADE_ODDS=1
# [ -n "$SEED" ] && RANDOM=$SEED
# [ -z "$ONEPER" -a $NROOMS -lt $NGAMES ] && usage "use --one-per if --num-rooms < --num-games"

# [ -n "$DO_CLEAN" ] && cleanup

# RESUME=""
# for FILE in $(ls $LOGDIR/*.{xwg,txt} 2>/dev/null); do
#     if [ -e $FILE ]; then
#         echo "Unfinished games found in $LOGDIR; continue with them (or discard)?"
#         read -p "<yes/no> " ANSWER
#         case "$ANSWER" in
#             y|yes|Y|YES)
#                 RESUME=1
#                 ;;
#             *)
#                 ;;
#         esac
#     fi
#     break
# done

# if [ -z "$RESUME" -a -d $LOGDIR ]; then
# 	NEWNAME="$(basename $LOGDIR)_$$"
#     (cd $(dirname $LOGDIR) && mv $(basename $LOGDIR) /tmp/${NEWNAME})
# fi
# mkdir -p $LOGDIR

# if [ "$SAVE_GOOD" = YES ]; then
#     DONEDIR=$LOGDIR/done
#     mkdir -p $DONEDIR
# fi
# DEADDIR=$LOGDIR/dead
# mkdir -p $DEADDIR

# for VAR in NGAMES NROOMS USE_GTK TIMEOUT HOST PORT SAVE_GOOD \
#     MINDEVS MAXDEVS ONEPER RESIGN_PCT DROP_N ALL_VIA_RQ SEED \
#     APP_NEW; do
#     echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
# done
# echo "DICTS: ${DICTS[*]}"
# echo -n "APPS_OLD: "; [ xx = "${APPS_OLD[*]+xx}" ] && echo "${APPS_OLD[*]}" || echo ""

# echo "*********$0 starting: $(date)**************"
# STARTTIME=$(date +%s)
# [ -z "$RESUME" ] && build_cmds || read_resume_cmds
# if [ TRUE = "$ALL_VIA_RQ" ]; then
#     run_via_rq
# else
#     run_cmds
# fi

# wait

# SECONDS=$(($(date +%s)-$STARTTIME))
# HOURS=$((SECONDS/3600))
# SECONDS=$((SECONDS%3600))
# MINUTES=$((SECONDS/60))
# SECONDS=$((SECONDS%60))
# echo "*********$0 finished: $(date) (took $HOURS:$MINUTES:$SECONDS)**************"

def termHandler(signum, frame):
    global gDone
    print('termHandler() called')
    gDone = True

def main():
    startTime = datetime.datetime.now()
    signal.signal(signal.SIGINT, termHandler)

    args = parseArgs()
    # Hack: old files confuse things. Remove is simple fix good for now
    if args.ADD_SMS:
        try: rmtree('/tmp/xw_sms')
        except: None
    devs = build_cmds(args)
    nDevs = len(devs)
    run_cmds(args, devs)
    print('{} finished; took {} for {} devices'.format(sys.argv[0], datetime.datetime.now() - startTime, nDevs))

##############################################################################
if __name__ == '__main__':
    main()
