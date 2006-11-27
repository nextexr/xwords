// -*-mode: C; fill-column: 80; c-basic-offset: 4; -*-
/****************************************************************************
 *									    *
 *	Copyright 1998-2001 by Eric House (xwords@eehouse.org).  All rights reserved.	    *
 *									    *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 ****************************************************************************/
#include <StringMgr.h>
#include <MemoryMgr.h>
#include <SoundMgr.h>
#include <TimeMgr.h>
#include <Form.h>
#include <FeatureMgr.h>                                                         
#include <unix_stdarg.h>
#ifdef XWFEATURE_FIVEWAY
# include <Hs.h>
#endif

#include "strutils.h"
#include "palmutil.h"
#include "xwords4defines.h"
#include "palmmain.h"
#include "comtypes.h"

#define MEMO "MemoDB"
#define LOGFILE "XWLogfile"

/*****************************************************************************
 * This is meant to be replaced by an actual beep....
 ****************************************************************************/
#if defined FEATURE_BEEP || defined XW_FEATURE_UTILS
void beep( void ) {
    SndPlaySystemSound( sndError );
} /* beep */
#endif

/*****************************************************************************
 * 
 ****************************************************************************/
MemPtr
getActiveObjectPtr( UInt16 objectID )
{
    FormPtr form = FrmGetActiveForm();
    XP_ASSERT( FrmGetObjectPtr( 
	form, FrmGetObjectIndex( form, objectID ) )!= NULL );
    return FrmGetObjectPtr( form, FrmGetObjectIndex( form, objectID ) );
} /* getActiveObjectPtr */

/*****************************************************************************
 *
 ****************************************************************************/
void
getObjectBounds( UInt16 objectID, RectangleType* rectP ) 
{
   FormPtr form = FrmGetActiveForm();
   FrmGetObjectBounds( form, FrmGetObjectIndex( form, objectID ), rectP );
} /* getObjectBounds */

#if defined XW_FEATURE_UTILS
/*****************************************************************************
 *
 ****************************************************************************/
void
setObjectBounds( UInt16 objectID, RectangleType* rectP ) 
{
   FormPtr form = FrmGetActiveForm();
   FrmSetObjectBounds( form, FrmGetObjectIndex( form, objectID ), rectP );
} /* getObjectBounds */

/*****************************************************************************
 *
 ****************************************************************************/
void
setBooleanCtrl( UInt16 objectID, Boolean isSet ) 
{
    CtlSetValue( getActiveObjectPtr( objectID ), isSet );
} /* setBooleanCtrl */

void
setFieldEditable( UInt16 objectID, Boolean editable )
{
    FieldPtr fld = getActiveObjectPtr( objectID );
    FieldAttrType attrs;

    FldGetAttributes( fld, &attrs );
    if ( attrs.editable != editable ) {
        attrs.editable = editable;
        FldSetAttributes( fld, &attrs );
    }
} /* setFieldEditable */

void
disOrEnable( FormPtr form, UInt16 id, Boolean enable )
{
    UInt16 index = FrmGetObjectIndex( form, id );
    FormObjectKind typ;

    if ( enable ) {
        FrmShowObject( form, index );
    } else {
        FrmHideObject( form, index );
    }

    typ = FrmGetObjectType( form, index );
    if ( typ == frmControlObj ) {
        ControlPtr ctl = getActiveObjectPtr( id );
        CtlSetEnabled( ctl, enable );
    }
} /* disOrEnable */

void
disOrEnableTri( FormPtr form, UInt16 id, XP_TriEnable enable )
{
    UInt16 index;
    XP_ASSERT( enable != TRI_ENAB_NONE );

    index = FrmGetObjectIndex( form, id );

    if ( enable == TRI_ENAB_HIDDEN ) {
        FrmHideObject( form, index );
    } else {
        FormObjectKind typ = FrmGetObjectType( form, index );
        XP_Bool active = enable == TRI_ENAB_ENABLED;

        FrmShowObject( form, index );

        switch( typ ) {
        case frmFieldObj:
            setFieldEditable( id, active );
            break;
        case frmControlObj:
            CtlSetEnabled( getActiveObjectPtr( id ), enable );
            break;
        case frmLabelObj:            /* what to do? */
            break;
        default:
            XP_WARNF( "%s: %d not handled", __FUNCTION__, (XP_U16)typ );
            XP_ASSERT(0);
        }
    }
} /* disOrEnableTri */

void
disOrEnableSet( FormPtr form, const UInt16* ids, Boolean enable ) 	 
{
    for ( ; ; ) {
        XP_U16 id = *ids++;
        if ( !id ) {
            break;
        }
        disOrEnable( form, id, enable );
    }
} /* disOrEnableSet */

/* Position a control at the horizontal center of its container.
 */
void
centerControl( FormPtr form, UInt16 id )
{
    RectangleType cBounds, fBounds;
    UInt16 index = FrmGetObjectIndex( form, id );

    FrmGetObjectBounds( form, index, &cBounds );
    FrmGetFormBounds( form, &fBounds );

    cBounds.topLeft.x = (fBounds.extent.x - cBounds.extent.x) / 2;
    FrmSetObjectBounds( form, index, &cBounds );
} /* centerButton */

/*****************************************************************************
 *
 ****************************************************************************/
Boolean 
getBooleanCtrl( UInt16 objectID ) 
{
    return CtlGetValue( getActiveObjectPtr( objectID ) );
} /* getBooleanCtrl */

void
setFieldStr( XP_U16 id, const XP_UCHAR* buf )
{
    FieldPtr field = getActiveObjectPtr( id );
    UInt16 len = FldGetTextLength( field );

    if ( !buf ) {
        buf = "";
    }

    FldSetSelection( field, 0, len );
    FldInsert( field, buf, XP_STRLEN(buf) );
} /* setFieldStr */

/*****************************************************************************
 * Set up to build the string and ptr-to-string lists needed for the
 * LstSetListChoices system call.
 ****************************************************************************/
void
initListData( MPFORMAL ListData* ld, XP_U16 nItems ) 
{
    /* include room for the closure */
    ld->strings = XP_MALLOC( mpool, ((nItems+1) * sizeof(*ld->strings)) );

    ld->storage = XP_MALLOC( mpool, 1 );
    ld->storage[0] = '\0';
    ld->storageLen = 1;

    ld->nItems = nItems;
    ld->nextIndex = 0;
    ld->selIndex = -1;
#ifdef DEBUG
    ld->choicesSet = XP_FALSE;
#endif
} /* initListData */

void 
addListTextItem( MPFORMAL ListData* ld, XP_UCHAR* txt ) 
{
    XP_U16 curLen = ld->storageLen;
    XP_U16 strLen = XP_STRLEN( txt ) + 1; /* null byte */
    XP_S32 diff;
    unsigned char* storage;
    XP_S16 i;

    storage = XP_REALLOC( mpool, ld->storage, curLen + strLen );
    XP_MEMCPY( storage + curLen, txt, strLen );

    /* Now update all the existing ptrs since storage may have moved.
       Remember to skip item 0. */
    diff = storage - ld->storage;
    for ( i = ld->nextIndex; i > 0; --i ) {
        ld->strings[i] += diff;
    }

    ld->storage = storage;
    ld->strings[++ld->nextIndex] = storage + curLen;
    ld->storageLen += strLen;
} /* addListTextItem */

/*****************************************************************************
 * Turn the list of offsets into ptrs, free the offsets list, and call
 * LstSetListChoices
 ****************************************************************************/
void 
setListChoices( ListData* ld, ListPtr list, void* closure ) 
{
    ld->strings[0] = closure;
    LstSetListChoices( list, (Char**)&ld->strings[1], ld->nextIndex );
#ifdef DEBUG
    ld->choicesSet = XP_TRUE;
#endif
    if ( ld->selIndex >= 0 ) {
        LstSetSelection( list, ld->selIndex );
    }
} /* setListChoices */

/*****************************************************************************
 * Given a string, figure out which item it matches and save off that item's
 * index so that at show time it can be used to set the selection.  Note that
 * anything that changes the order will invalidate this, but also that there's
 * no harm in calling it again.
 ****************************************************************************/
void 
setListSelection( ListData* ld, char* selName )
{
    ld->selIndex = 0;
    XP_ASSERT( !ld->choicesSet );

    if ( !!selName ) {
        XP_U16 i;
        for ( i = 0; i < ld->nextIndex; ++i ) {
            if ( StrCompare( ld->strings[i+1], selName ) == 0 ) {
                ld->selIndex = i;
                break;
            }
        }
    }
} /* setListSelection */

/*****************************************************************************
 * Meant to be called after all items are added to the list, this function
 * sorts them in place.  For now I'll build in a call to StrCompare; later
 * it could be a callback passed in.
 ****************************************************************************/
void
sortList( ListData* ld ) 
{
    XP_S16 i, j, smallest;
    unsigned char** strings = ld->strings;
    char* tmp;

    XP_ASSERT( !ld->choicesSet ); /* if ARM, strings are reversed.  Use list
                                     API at this point. */

    for ( i = 1; i <= ld->nextIndex; ++i ) { /* skip 0th (closure) slot */
        for ( smallest = i, j = i+1; j <= ld->nextIndex; ++j ) {
            if ( StrCompare( strings[smallest], strings[j] ) > 0 ) {
                smallest = j;
            }
        }

        if ( smallest == i ) {	/* we got to the end without finding anything */
            break;
        }

        tmp = strings[i];
        strings[i] = strings[smallest];
        strings[smallest] = tmp;
    }
} /* sortList */

/*****************************************************************************
 * Dispose the memory.  Docs don't say whether LstSetListChoices does this for
 * me so I assume not.  It'll crash if I'm wrong. :-)
 ****************************************************************************/
void
freeListData( MPFORMAL ListData* ld  ) 
{
    XP_FREE( mpool, ld->storage );
    XP_FREE( mpool, ld->strings );
} /* freeListData */

/*****************************************************************************
 *
 ****************************************************************************/
void 
setSelectorFromList( UInt16 triggerID, ListPtr list, XP_S16 listSelIndex ) 
{
    XP_ASSERT( list != NULL );
    XP_ASSERT( getActiveObjectPtr( triggerID ) != NULL );
    XP_ASSERT( !!LstGetSelectionText( list, listSelIndex ) );
    CtlSetLabel( getActiveObjectPtr( triggerID ),
		 LstGetSelectionText( list, listSelIndex ) );
} /* setTriggerFromList */

XP_Bool
penInGadget( const EventType* event, UInt16* whichGadget )
{
    UInt16 x = event->screenX;
    UInt16 y = event->screenY;
    FormPtr form = FrmGetActiveForm();
    UInt16 nObjects, i;
    XP_Bool result = XP_FALSE;

    for ( i = 0, nObjects = FrmGetNumberOfObjects(form); i < nObjects; ++i ) {
        if ( frmGadgetObj == FrmGetObjectType( form, i ) ) {
            UInt16 objId = FrmGetObjectId( form, i );
            if ( objId != REFCON_GADGET_ID ) {
                RectangleType rect;
                FrmGetObjectBounds( form, i, &rect );
                if ( RctPtInRectangle( x, y, &rect ) ) {
                    *whichGadget = objId;
                    result = XP_TRUE;
                    break;
                }
            }
        }
    }
    
    return result;
} /* penInGadget */

void
drawOneGadget( UInt16 id, const char* text, Boolean hilite )
{
    RectangleType divRect;
    XP_U16 len = XP_STRLEN(text);
    XP_U16 width = FntCharsWidth( text, len );
    XP_U16 left;

    getObjectBounds( id, &divRect );
    WinDrawRectangleFrame( rectangleFrame, &divRect );
    WinEraseRectangle( &divRect, 0 );
    left = divRect.topLeft.x;
    left += (divRect.extent.x - width) / 2;
    WinDrawChars( text, len, left, divRect.topLeft.y );
    if ( hilite ) {
        WinInvertRectangle( &divRect, 0 );
    }
} /* drawOneGadget */

#ifdef XWFEATURE_FIVEWAY
XP_S16
getFocusOwner( void )
{
    FormPtr form = FrmGetActiveForm();
    XP_S16 ownerID = -1;
    XP_S16 focus = FrmGetFocus( form );
    if ( focus >= 0 ) {
        ownerID = FrmGetObjectId( form, focus );
    }
    return ownerID;
} /* getFocusOwner */

void
drawFocusRingOnGadget( XP_U16 idLow, XP_U16 idHigh )
{
#ifndef XW_TARGET_PNO           /* temporary: I need to figure out how to call
                                   HsNavDrawFocusRing from ARM code */
    FormPtr form;
    XP_S16 index;
    XP_U16 focusID;

    LOG_FUNC();

    form = FrmGetActiveForm();
    index = FrmGetFocus( form );
    XP_LOGF( "%s: FrmGetFocus=>%d", __FUNCTION__, index );
    if ( index >= 0 ) {
        focusID = FrmGetObjectId( form, index );
        XP_LOGF( "%s: FrmGetObjectId=>%d", __FUNCTION__, focusID );

        if ( (focusID >= idLow) && (focusID <= idHigh) ) {
            Err err;
            RectangleType rect;

            getObjectBounds( focusID, &rect );
            XP_LOGF( "focusID=%d; index=%d", focusID, index );
            XP_LOGF( "rect=%d,%d,%d,%d", rect.topLeft.x, rect.topLeft.y,
                     rect.extent.x, rect.extent.y );

            /* growing the rect didn't work to fix glitches in ring drawing. */

            err = HsNavDrawFocusRing( form, focusID, 0, &rect,
                                      hsNavFocusRingStyleObjectTypeDefault,
                                      false );
            if ( err != errNone ) {
                XP_LOGF( "%s: err=%d (0x%x)", __FUNCTION__, err, err );
            }
            XP_ASSERT( err == errNone ); /* firing */
        }
    }
    LOG_RETURN_VOID();
#endif
} /* drawFocusRingOnGadget */

XP_Bool
considerGadgetFocus( const EventType* event, XP_U16 idLow, XP_U16 idHigh )
{
    XP_Bool handled;
    XP_U16 objectID;

    XP_ASSERT( event->eType == frmObjectFocusLostEvent
               || event->eType == frmObjectFocusTakeEvent );
    XP_ASSERT( event->data.frmObjectFocusTake.formID == FrmGetActiveFormID() );
    XP_ASSERT( &event->data.frmObjectFocusTake.objectID
               == &event->data.frmObjectFocusLost.objectID );

    objectID = event->data.frmObjectFocusTake.objectID;
    XP_LOGF( "%s: objectID=%d", __FUNCTION__, objectID );
    handled = (objectID >= idLow) && (objectID <= idHigh);
    if ( handled ) {
        if ( event->eType == frmObjectFocusTakeEvent ) {
            FormPtr form = FrmGetActiveForm();
            FrmSetFocus( form, FrmGetObjectIndex(form, objectID) );
            drawFocusRingOnGadget( idLow, idHigh );
        }
    }

    LOG_RETURNF( "%d", (XP_U16)handled );
    return handled;
} /* considerGadgetFocus */

XP_Bool
tryRockerKey( XP_U16 key, XP_U16 selGadget, XP_U16 idLow, XP_U16 idHigh )
{
    XP_Bool result = XP_FALSE;

    if ( vchrRockerCenter == key ) {
        if ( selGadget >= idLow && selGadget <= idHigh ) {
            result = XP_TRUE;
        }
    }
    return result;
} /* tryRockerKey */
#endif

void
drawGadgetsFromList( ListPtr list, XP_U16 idLow, XP_U16 idHigh, 
                     XP_U16 hiliteItem )
{
    XP_U16 i;
    LOG_FUNC();
    XP_ASSERT( idLow <= idHigh );

    for ( i = 0; idLow <= idHigh; ++i, ++idLow ) {
        const char* text = LstGetSelectionText( list, i );
        Boolean hilite = idLow == hiliteItem;
        drawOneGadget( idLow, text, hilite );
    }
    LOG_RETURN_VOID();
} /* drawGadgetsFromList */

void
setFormRefcon( void* refcon )
{
#ifdef DEBUG
    Err err = 
#endif
        FtrSet( APPID, GLOBALS_FEATURE, (UInt32)refcon );
    XP_ASSERT( err == errNone );
} /* setFormRefcon */

void* 
getFormRefcon()
{
    UInt32 ptr;
#ifdef DEBUG
    Err err = 
#endif
        FtrGet( APPID, GLOBALS_FEATURE, &ptr );
    XP_ASSERT( err == errNone );
    XP_ASSERT( ptr != 0L );
    return (void*)ptr;
} /* getFormRefcon */

void
fitButtonToString( XP_U16 id )
{
    ControlPtr button = getActiveObjectPtr( id );
    const char* label = CtlGetLabel( button );
    XP_U16 width = FntCharsWidth( label, XP_STRLEN(label) );
    RectangleType rect;
    width += 14;                /* 7 pixels at either end */

    getObjectBounds( id, &rect );
    rect.topLeft.x -= (rect.extent.x - width);
    rect.extent.x = width;

    setObjectBounds( id, &rect );
} /* fitButtonToString */

#endif

#if defined FEATURE_REALLOC || defined XW_FEATURE_UTILS
XP_U8* 
palm_realloc( XP_U8* in, XP_U16 size )
{
    MemPtr ptr = (MemPtr)in;
    XP_U32 oldsize = MemPtrSize( ptr );
    MemPtr newptr = MemPtrNew( size );
    XP_ASSERT( !!newptr );
    XP_MEMCPY( newptr, ptr, oldsize );
    MemPtrFree( ptr );
    return newptr;
} /* palm_realloc */

XP_U16
palm_snprintf( XP_UCHAR* buf, XP_U16 XP_UNUSED_DBG(len), 
               XP_UCHAR* format, ... )
{
    XP_U16 nChars;
    /* PENDING use len to avoid writing too many chars */
    va_list ap;
    
    va_start( ap, format );
    nChars = StrVPrintF( (char*)buf, (char*)format, ap );
    va_end( ap );
    XP_ASSERT( len >= nChars );
    return nChars;
} /* palm_snprintf */
#endif

#ifdef FOR_GREMLINS
static Boolean 
doNothing( EventPtr event )
{
    return true;
} /* doNothing */
#endif

#ifdef DEBUG
void
logEvent( eventsEnum eType )
{
    char* name = NULL;
#define CASE_STR(e) case e: name = #e; break
    switch( eType ) {
        CASE_STR(nilEvent);
        CASE_STR(penDownEvent);
        CASE_STR(penUpEvent);
        CASE_STR(penMoveEvent);
        CASE_STR(keyDownEvent);
        CASE_STR(winEnterEvent);
        CASE_STR(winExitEvent);
        CASE_STR(ctlEnterEvent);
        CASE_STR(ctlExitEvent);
        CASE_STR(ctlSelectEvent);
        CASE_STR(ctlRepeatEvent);
        CASE_STR(lstEnterEvent);
        CASE_STR(lstSelectEvent);
        CASE_STR(lstExitEvent);
        CASE_STR(popSelectEvent);
        CASE_STR(fldEnterEvent);
        CASE_STR(fldHeightChangedEvent);
        CASE_STR(fldChangedEvent);
        CASE_STR(tblEnterEvent);
        CASE_STR(tblSelectEvent);
        CASE_STR(daySelectEvent);
        CASE_STR(menuEvent);
        CASE_STR(appStopEvent);
        CASE_STR(frmLoadEvent);
        CASE_STR(frmOpenEvent);
        CASE_STR(frmGotoEvent);
        CASE_STR(frmUpdateEvent);
        CASE_STR(frmSaveEvent);
        CASE_STR(frmCloseEvent);
        CASE_STR(frmTitleEnterEvent);
        CASE_STR(frmTitleSelectEvent);
        CASE_STR(tblExitEvent);
        CASE_STR(sclEnterEvent);
        CASE_STR(sclExitEvent);
        CASE_STR(sclRepeatEvent);
        CASE_STR(tsmConfirmEvent);
        CASE_STR(tsmFepButtonEvent);
        CASE_STR(tsmFepModeEvent);
        CASE_STR(attnIndicatorEnterEvent);
        CASE_STR(attnIndicatorSelectEvent);
        CASE_STR(menuCmdBarOpenEvent);
        CASE_STR(menuOpenEvent);
        CASE_STR(menuCloseEvent);
        CASE_STR(frmGadgetEnterEvent);
        CASE_STR(frmGadgetMiscEvent);

        CASE_STR(firstINetLibEvent);
        CASE_STR(firstWebLibEvent);
        CASE_STR(telAsyncReplyEvent); 

        CASE_STR(keyUpEvent);
        CASE_STR(keyHoldEvent);
        CASE_STR(frmObjectFocusTakeEvent);
        CASE_STR(frmObjectFocusLostEvent);

        CASE_STR(firstLicenseeEvent);
        CASE_STR(lastLicenseeEvent);

        CASE_STR(firstUserEvent);
        CASE_STR(lastUserEvent);
    }
#undef CASE_STR
    if ( !!name ) {
        XP_LOGF( "eType = %s", name );
    }
} /* logEvent */

void
palm_warnf( char* format, ... )
{
    char buf[200];
    va_list ap;
    
    va_start( ap, format );
    StrVPrintF( buf, format, ap );
    va_end( ap );

#ifdef FOR_GREMLINS
    /* If gremlins are active, we want all activity to stop here!  That
       means no cancellation */
    {
        FormPtr form;
        FieldPtr field;

        form = FrmInitForm( XW_GREMLIN_WARN_FORM_ID );

        FrmSetActiveForm( form );

        FrmSetEventHandler( form, doNothing );

        field = getActiveObjectPtr( XW_GREMLIN_WARN_FIELD_ID );
        FldSetTextPtr( field, buf );
        FldRecalculateField( field, true );

        FrmDrawForm( form );
#if 1
        /* This next may freeze X (go to a console to kill POSE), but when
           you look at the logs you'll see what event you were on when the
           ASSERT fired.  Otherwise the events just keep coming and POSE
           fills up the log.  */
        while(1);
#endif
        /* this should NEVER return */
        (void)FrmDoDialog( form );
    }
#else
    (void)FrmCustomAlert( XW_ERROR_ALERT_ID, buf, " ", " " );
#endif
} /* palm_warnf */

void
palm_assert( Boolean b, int line, const char* func, const char* file )
{
    if ( !b ) {
        /* force file logging on if not already */
        FtrSet( APPID, LOG_FILE_FEATURE, 1 );
        XP_LOGF( "ASSERTION FAILED: line %d, %s(), %s", line, func, file );

        XP_WARNF( "ASSERTION FAILED: line %d, %s(), %s", line, func, file );
    }
} /* palmassert */

static void
logToDB( const char* buf, const char* dbName, XP_U32 dbCreator, XP_U32 dbType )
{
    const XP_U16 MAX_MEMO_SIZE = 4000;
    const XP_U16 MAX_NRECORDS = 200;
    DmOpenRef ref;
    UInt16 nRecords, index;
    UInt16 len = XP_STRLEN( buf );
    UInt16 hSize, slen;
    MemHandle hand;
    MemPtr ptr;
    char tsBuf[20];
    UInt16 tsLen;
    DateTimeType dtType;
    LocalID dbID;

    TimSecondsToDateTime( TimGetSeconds(), &dtType );
    StrPrintF( tsBuf, "\n%d:%d:%d-", dtType.hour, dtType.minute, 
               dtType.second );
    tsLen = XP_STRLEN(tsBuf);

    (void)DmCreateDatabase( CARD_0, dbName, dbCreator, dbType, false );
    dbID = DmFindDatabase( CARD_0, dbName );
    ref = DmOpenDatabase( CARD_0, dbID, dmModeWrite );

    nRecords = DmNumRecordsInCategory( ref, dmAllCategories );
    if ( nRecords == 0 ) {
        index = dmMaxRecordIndex;
        hSize = 0;
        hand = DmNewRecord( ref, &index, 1 );
        DmReleaseRecord( ref, index, true );
    } else {

        while ( nRecords > MAX_NRECORDS ) {
            index = 0;
            DmSeekRecordInCategory( ref, &index, 0, dmSeekForward, 
                                    dmAllCategories);
            DmRemoveRecord( ref, index );
            --nRecords;
        }

        index = 0;
        DmSeekRecordInCategory( ref, &index, nRecords, dmSeekForward, 
                                dmAllCategories);
        hand = DmGetRecord( ref, index );

        XP_ASSERT( !!hand );
        hSize = MemHandleSize( hand ) - 1;
        ptr = MemHandleLock( hand );
        slen = XP_STRLEN(ptr);
        MemHandleUnlock(hand);

        if ( hSize > slen ) {
            hSize = slen;
        }
        (void)DmReleaseRecord( ref, index, false );
    }

    if ( (hSize + len + tsLen) > MAX_MEMO_SIZE ) {
        index = dmMaxRecordIndex;
        hand = DmNewRecord( ref, &index, len + tsLen + 1 );
        hSize = 0;
    } else {
        (void)DmResizeRecord( ref, index, len + hSize + tsLen + 1 );
        hand = DmGetRecord( ref, index );
    }

    ptr = MemHandleLock( hand );
    DmWrite( ptr, hSize, tsBuf, tsLen );
    DmWrite( ptr, hSize + tsLen, buf, len + 1 );
    MemHandleUnlock( hand );

    DmReleaseRecord( ref, index, true );
    DmCloseDatabase( ref );
} /* logToDB */

static void
deleteDB( const char* dbName ) 
{
    LocalID dbID;
 	dbID = DmFindDatabase( CARD_0, dbName );
    if ( 0 != dbID ) {
		Err err = DmDeleteDatabase( CARD_0, dbID );
        XP_ASSERT( errNone == err );
    } else {
        XP_WARNF( "%s(%s): got back 0", __FUNCTION__, dbName );
    }
} /* deleteDB */

void
PalmClearLogs( void )
{
    deleteDB( MEMO );
    deleteDB( LOGFILE );
}

static void
logToMemo( const char* buf )
{
    UInt32 val = 0L;
    Err err = FtrGet( APPID, LOG_MEMO_FEATURE, (UInt32*)&val );
    if ( errNone == err && val != 0 ) {
        logToDB( buf, MEMO, 'memo', 'DATA' );
    }
}

static void
logToFile( const char* buf )
{
    UInt32 val = 0L;
    Err err = FtrGet( APPID, LOG_FILE_FEATURE, (UInt32*)&val );
    if ( errNone == err && val != 0 ) {
        logToDB( buf, LOGFILE, 'XWLG', 'TEXT' );
    }
}

void
palm_debugf( char* format, ...)
{
    char buf[200];
    va_list ap;
    
    va_start( ap, format );
    StrVPrintF( buf, format, ap );
    va_end( ap );

    logToMemo( buf );
    logToFile( buf );
} /* debugf */

void
palm_logf( char* format, ... )
{
    char buf[200];
    va_list ap;
    
    va_start( ap, format );
    StrVPrintF( buf, format, ap );
    va_end( ap );

    logToMemo( buf );
    logToFile( buf );
} /* palm_logf */

#endif /* DEBUG */
