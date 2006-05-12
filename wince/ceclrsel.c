/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2004 by Eric House (xwords@eehouse.org).  All rights reserved.
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
 */

#include <windowsx.h>
#include "stdafx.h" 
#include <commdlg.h>

#include "ceclrsel.h" 
#include "ceutil.h" 
#include "debhacks.h"

#ifdef MY_COLOR_SEL

typedef struct ClrEditDlgState {
    CEAppGlobals* globals;

    RECT clrRect;

    XP_U8 r;
    XP_U8 b;
    XP_U8 g;

    XP_Bool inited;
    XP_Bool cancelled;
} ClrEditDlgState;

static void
drawColorRect( ClrEditDlgState* eState, HDC hdc )
{
    COLORREF ref = RGB( eState->r, eState->g, eState->b );
    HBRUSH brush = CreateSolidBrush( ref );
    FillRect( hdc, &eState->clrRect, brush );
    DeleteObject( brush );
} /* drawColorRect */

static void
initEditAndSlider( HWND hDlg, XP_U16 sliderID, XP_U8 val )
{
    SendDlgItemMessage( hDlg, sliderID, TBM_SETRANGE, TRUE, 
                        MAKELONG(0,255) );
    SendDlgItemMessage( hDlg, sliderID, TBM_SETPOS, TRUE, 
                        (long)val );
    ceSetDlgItemNum( hDlg, sliderID+1, val );
} /* initEditAndSlider */

static void
initChooseColor( ClrEditDlgState* eState, HWND hDlg )
{
    eState->clrRect.left = 162;
    eState->clrRect.top = 5;
    eState->clrRect.right = 193;
    eState->clrRect.bottom = 90;

    InvalidateRect( hDlg, &eState->clrRect, FALSE );

    initEditAndSlider( hDlg, CLREDT_SLIDER1, eState->r );
    initEditAndSlider( hDlg, CLREDT_SLIDER2, eState->g );
    initEditAndSlider( hDlg, CLREDT_SLIDER3, eState->b );
} /* initChooseColor */

static XP_U8*
colorForSlider( ClrEditDlgState* eState, XP_U16 sliderID )
{
    switch( sliderID ) {
    case CLREDT_SLIDER1:
        return &eState->r;
    case CLREDT_SLIDER2:
        return &eState->g;
    case CLREDT_SLIDER3:
        return &eState->b;
    default:
        XP_LOGF( "huh???" );
        return NULL;
    }
} /* colorForSlider */

static void 
updateForSlider( HWND hDlg, ClrEditDlgState* eState, XP_U16 sliderID )
{
    XP_U8 newColor = (XP_U8)SendDlgItemMessage( hDlg, sliderID, TBM_GETPOS, 
                                                0, 0L );
    XP_U8* colorPtr = colorForSlider( eState, sliderID );
    if ( newColor != *colorPtr ) {
        *colorPtr = newColor;

        ceSetDlgItemNum( hDlg, sliderID+1, (XP_S32)newColor );

        InvalidateRect( hDlg, &eState->clrRect, FALSE );
    }
} /* updateForSlider */

static void
updateForField( HWND hDlg, ClrEditDlgState* eState, XP_U16 fieldID )
{
    XP_S32 newColor = ceGetDlgItemNum( hDlg, fieldID );
    XP_U8* colorPtr = colorForSlider( eState, fieldID - 1 );
    XP_Bool modified = XP_FALSE;;

    if ( newColor > 255 ) {
        newColor = 255;
        modified = XP_TRUE;
    } else if ( newColor < 0 ) {
        newColor = 0;
        modified = XP_TRUE;
    } 
    if ( modified ) {
        ceSetDlgItemNum( hDlg, fieldID, newColor );
    }
    
    if ( newColor != *colorPtr ) {
        *colorPtr = (XP_U8)newColor;

        SendDlgItemMessage( hDlg, fieldID-1, TBM_SETPOS, TRUE, 
                            (long)newColor );
        InvalidateRect( hDlg, &eState->clrRect, FALSE );
    }
} /* updateForField */

LRESULT CALLBACK
EditColorsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ClrEditDlgState* eState;
    XP_U16 wid;
    XP_U16 notifyCode;
    NMTOOLBAR* nmToolP; 

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );

        eState = (ClrEditDlgState*)lParam;
        eState->cancelled = XP_TRUE;
        eState->inited = XP_FALSE;

        return TRUE;
    } else {
        eState = (ClrEditDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !eState ) {
            return FALSE;
        }

        if ( !eState->inited ) {
            /* set to true first! Messages will be generated by
               initChooseColor call below */
            eState->inited = XP_TRUE;
            initChooseColor( eState, hDlg );
            XP_LOGF( "initChooseColor done" );
        }

        switch (message) {

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint( hDlg, &ps );
            drawColorRect( eState, hdc );
            EndPaint( hDlg, &ps );
        }
            break;

        case WM_NOTIFY:
            nmToolP = (NMTOOLBAR*)lParam;
            wid = nmToolP->hdr.idFrom;
            switch ( wid ) {
            case CLREDT_SLIDER1:
            case CLREDT_SLIDER2:
            case CLREDT_SLIDER3:
                updateForSlider( hDlg, eState, wid );
                break;
            }
            break;

        case WM_COMMAND:
            wid = LOWORD(wParam);
            switch( wid ) {
            case RED_EDIT:
            case GREEN_EDIT:
            case BLUE_EDIT:
                notifyCode = HIWORD(wParam);
                if ( notifyCode == EN_CHANGE ) {
                    updateForField( hDlg, eState, wid );
                    return TRUE;
                }
                break;

            case IDOK:
                eState->cancelled = XP_FALSE;
                /* fallthrough */
                
            case IDCANCEL:
                EndDialog(hDlg, wid);
                return TRUE;
            }
        }
    }

    return FALSE;
} /* EditColorsDlg */

static XP_Bool
myChooseColor( CEAppGlobals* globals, HWND hwnd, COLORREF* cref )
{
    ClrEditDlgState state;
    int result;

    XP_MEMSET( &state, 0, sizeof(state) );
    state.globals = globals;
    state.r = GetRValue(*cref);
    state.g = GetGValue(*cref);
    state.b = GetBValue(*cref);

    XP_LOGF( "setting up IDD_COLOREDITDLG" );

    result = DialogBoxParam( globals->hInst, (LPCTSTR)IDD_COLOREDITDLG, hwnd,
                             (DLGPROC)EditColorsDlg, (long)&state );

    XP_LOGF( "DialogBoxParam=>%d", result );

    if ( !state.cancelled ) {
        *cref = RGB( state.r, state.g, state.b );
    }
        
    return !state.cancelled;
} /* myChooseColor */

#endif /* MY_COLOR_SEL */

typedef struct ColorsDlgState {

    CEAppGlobals* globals;
    COLORREF* inColors;

    COLORREF colors[NUM_EDITABLE_COLORS];
    HBRUSH brushes[NUM_EDITABLE_COLORS];
    HWND buttons[NUM_EDITABLE_COLORS];

    XP_Bool cancelled;
    XP_Bool inited;
} ColorsDlgState;

#define FIRST_BUTTON DLBLTR_BUTTON
#define LAST_BUTTON PLAYER4_BUTTON

static void
initColorData( ColorsDlgState* cState, HWND hDlg )
{
    XP_U16 i;

    XP_ASSERT( (LAST_BUTTON - FIRST_BUTTON + 1) == NUM_EDITABLE_COLORS );

    for ( i = 0; i < NUM_EDITABLE_COLORS; ++i ) {
        COLORREF ref = cState->inColors[i];
        cState->colors[i] = ref;
        cState->brushes[i] = CreateSolidBrush( ref );
        cState->buttons[i] = GetDlgItem( hDlg, FIRST_BUTTON + i );
    }
} /* initColorData */

static HBRUSH
brushForButton( ColorsDlgState* cState, HWND hwndButton )
{
    XP_U16 i;
    for ( i = 0; i < NUM_EDITABLE_COLORS; ++i ) {
        if ( cState->buttons[i] == hwndButton ) {
            return cState->brushes[i];
        }
    }
    return NULL;
} /* brushForButton */

static void
deleteButtonBrushes( ColorsDlgState* cState )
{
    XP_U16 i;
    for ( i = 0; i < NUM_EDITABLE_COLORS; ++i ) {
        DeleteObject( cState->brushes[i] );
    }
} /* deleteButtonBrushes */

static void
wrapChooseColor( ColorsDlgState* cState, HWND owner, XP_U16 button )
{
    XP_U16 index = button-FIRST_BUTTON;

#ifdef MY_COLOR_SEL
    COLORREF clrref = cState->colors[index];

    if ( myChooseColor( cState->globals, owner, &clrref ) ) {
        cState->colors[index] = clrref;
        DeleteObject( cState->brushes[index] );
        cState->brushes[index] = CreateSolidBrush( clrref );
        XP_LOGF( "%s: may need to invalidate the button since color's changed", 
                 __FUNCTION__ );
    }
#else
    CHOOSECOLOR ccs;
    BOOL hitOk;
    COLORREF arr[16];
    XP_U16 i;

    XP_MEMSET( &ccs, 0, sizeof(ccs) );
    XP_MEMSET( &arr, 0, sizeof(arr) );

    for ( i = 0; i < NUM_EDITABLE_COLORS; ++i ) {
        arr[i] = cState->colors[i];
    }

    ccs.lStructSize = sizeof(ccs);
    ccs.hwndOwner = owner;
    ccs.rgbResult = cState->colors[index];
    ccs.lpCustColors = arr;

    ccs.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;

    hitOk = ChooseColor( &ccs );

    if ( hitOk ) {
        cState->colors[index] = ccs.rgbResult;
        DeleteObject( cState->brushes[index] );
        cState->brushes[index] = CreateSolidBrush( ccs.rgbResult );
    }
#endif
} /* wrapChooseColor */

static void
ceDrawColorButton( ColorsDlgState* cState, DRAWITEMSTRUCT* dis )
{
    HBRUSH brush = brushForButton( cState, dis->hwndItem );
    XP_ASSERT( !!brush );
    FillRect( dis->hDC, &dis->rcItem, brush );
} /* ceDrawColorButton */

LRESULT CALLBACK
ColorsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ColorsDlgState* cState;
    XP_U16 wid;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );

        cState = (ColorsDlgState*)lParam;
        cState->cancelled = XP_TRUE;
        cState->inited = XP_FALSE;

        result = TRUE;
    } else {
        cState = (ColorsDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!cState ) {

            if ( !cState->inited ) {
                initColorData( cState, hDlg );
                cState->inited = XP_TRUE;
            }

            switch (message) {

            case WM_DRAWITEM:   /* passed when button has BS_OWNERDRAW style */
                ceDrawColorButton( cState, (DRAWITEMSTRUCT*)lParam );
                result = TRUE;
                break;

            case WM_COMMAND:
                wid = LOWORD(wParam);
                switch( wid ) {

                case IDOK:
                    cState->cancelled = XP_FALSE;
                    /* fallthrough */

                case IDCANCEL:
                    deleteButtonBrushes( cState );
                    EndDialog(hDlg, wid);
                    result = TRUE;
                    break;
                default:
                    /* it's one of the color buttons.  Set up with the
                       appropriate color and launch ChooseColor */
                    wrapChooseColor( cState, hDlg, wid );
                    result = TRUE;
                    break;
                }

            }
        }
    }

    return result;
} /* ColorsDlg */

XP_Bool
ceDoColorsEdit( HWND hwnd, CEAppGlobals* globals, COLORREF* colors )
{
    ColorsDlgState state;

    XP_MEMSET( &state, 0, sizeof(state) );
    state.globals = globals;
    state.inColors = colors;

    (void)DialogBoxParam( globals->hInst, (LPCTSTR)IDD_COLORSDLG, hwnd,
                          (DLGPROC)ColorsDlg, (long)&state );

    if ( !state.cancelled ) {
        XP_U16 i;
        for ( i = 0; i < NUM_EDITABLE_COLORS; ++i ) {
            colors[i] = state.colors[i];
        }
    }
        
    return !state.cancelled;
} /* ceDoColorsEdit */
