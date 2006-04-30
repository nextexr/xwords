#!/usr/bin/perl

# Script for turning the Crosswords executable into a cab, along with
# a shortcut in the games menu

use strict;

my $userName = "Crosswords.exe";

sub main() {
    my $provider = "\"Crosswords project\"";

    usage() if 1 != @ARGV;

    my $path = shift @ARGV;

    usage() unless $path =~ m|.exe$|;

    die "$path not found\n" unless -f $path;

    my $cabname = `basename $path`;
    chomp $cabname;

    # Create a link.  The format, says Shaun, is
    # <number of characters>#command line<no carriage return or line feed>

    $userName = $cabname unless $userName;
    my $cmdline = "\"\\Program Files\\Crosswords\\" . $userName . "\"";
    my $cmdlen = length( $cmdline );

    $cabname =~ s/.exe$//;
    my $linkName = "Crosswords.lnk";
    open LINKFILE, "> $linkName";
    print LINKFILE $cmdlen, "#", $cmdline;
    close LINKFILE;

    my $fname = "/tmp/file$$.list";

# see this url for %CE5% and other definitions:
# http://msdn.microsoft.com/library/default.asp?url=/library/en-us/DevGuideSP/html/sp_wce51consmartphonewindowscestringsozup.asp

    open FILE, "> $fname";

    my $tmpfile = "/tmp/$userName";
    `cp $path $tmpfile`;
    print FILE "$tmpfile ";
    print FILE '%CE1%\\Crosswords', "\n";

    print FILE "../dawg/English/BasEnglish2to8.xwd ";
    print FILE '%CE1%\\Crosswords', "\n";

    print FILE "$linkName ";
    print FILE '%CE14%', "\n";

    close FILE;

    my $appname = $cabname;
    $cabname .= "_exe.cab";

    print( STDERR "pocketpc-cab -p $provider " ,
           "-a $appname $fname $cabname", "\n");
    my $cmd = "pocketpc-cab  -p $provider -a $appname "
        . "$fname $cabname";
    `$cmd`;

    unlink $linkName, $tmpfile;
}

sub usage() {
    print STDERR "usage: $0 path/to/xwords4.exe\n";
    exit 2;
}

main();
