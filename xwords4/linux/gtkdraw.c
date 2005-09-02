/* -*- mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 1997-2005 by Eric House (fixin@peak.org).  All rights reserved.
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
#ifdef PLATFORM_GTK

#include <stdlib.h>
#include <stdio.h>

#include <gdk/gdkdrawable.h>

#include "gtkmain.h"
#include "draw.h"
#include "board.h"
#include "linuxmain.h"

typedef enum {
    XP_GTK_JUST_NONE
    ,XP_GTK_JUST_CENTER
    ,XP_GTK_JUST_TOPLEFT
    ,XP_GTK_JUST_BOTTOMRIGHT
} XP_GTK_JUST;

/* static GdkGC* newGCForColor( GdkWindow* window, XP_Color* newC ); */
static void
insetRect( XP_Rect* r, short i )
{
    r->top += i;
    r->left += i;
    i *= 2;

    r->width -= i;
    r->height -= i;
} /* insetRect */

#if 0
#define DRAW_WHAT(dc) ((dc)->globals->pixmap)
#else
#define DRAW_WHAT(dc) ((dc)->drawing_area->window)
#endif


static void
eraseRect(GtkDrawCtx* dctx, XP_Rect* rect )
{
    gdk_draw_rectangle( DRAW_WHAT(dctx),
                        dctx->drawing_area->style->white_gc,
                        TRUE, rect->left, rect->top, 
                        rect->width, rect->height );
} /* eraseRect */

static void
frameRect( GtkDrawCtx* dctx, XP_Rect* rect )
{
    gdk_draw_rectangle( DRAW_WHAT(dctx),
                        dctx->drawGC, FALSE, rect->left, rect->top, 
                        rect->width, rect->height );
} /* frameRect */

#ifdef DRAW_WITH_PRIMITIVES

static void
gtk_prim_draw_setClip( DrawCtx* p_dctx, XP_Rect* newClip, XP_Rect* oldClip)
{
} /* gtk_prim_draw_setClip */

static void 
gtk_prim_draw_frameRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    frameRect( dctx, rect );
} /* gtk_prim_draw_frameRect */

static void
gtk_prim_draw_invertRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    /* not sure you can do this on GTK!! */
} /* gtk_prim_draw_invertRect */

static void
gtk_prim_draw_clearRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    eraseRect( dctx, rect );
} /* gtk_prim_draw_clearRect */

static void
gtk_prim_draw_drawString( DrawCtx* p_dctx, XP_UCHAR* str,
                          XP_U16 x, XP_U16 y )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_U16 fontHeight = 10;     /* FIX ME */
    gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                     x, y + fontHeight, str );
} /* gtk_prim_draw_drawString */

static void
gtk_prim_draw_drawBitmap( DrawCtx* p_dctx, XP_Bitmap bm, 
                          XP_U16 x, XP_U16 y )
{
} /* gtk_prim_draw_drawBitmap */

static void
gtk_prim_draw_measureText( DrawCtx* p_dctx, XP_UCHAR* str, 
                           XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    gint len = strlen(str);
    gint width = gdk_text_measure( dctx->gdkFont, str, len );

    *widthP = width;
    *heightP = 12;              /* ??? :-) */
} /* gtk_prim_draw_measureText */

#endif /* DRAW_WITH_PRIMITIVES */

static void
draw_string_at( GtkDrawCtx* dctx, PangoLayout* layout, const char* str, 
                const XP_Rect* where, XP_GTK_JUST just,
                const GdkColor* frground, const GdkColor* bkgrnd )
{
    XP_U16 x = where->left;
    XP_U16 y = where->top;

    pango_layout_set_text( layout, str, strlen(str) );

    if ( just != XP_GTK_JUST_NONE ) {
        int width, height;
        pango_layout_get_pixel_size( layout, &width, &height );

        switch( just ) {
        case XP_GTK_JUST_CENTER:
            x += (where->width - width) / 2;
            if ( where->height > height) {
                y += (where->height - height) / 2;
            }
            break;
        case XP_GTK_JUST_BOTTOMRIGHT:
            x += where->width - width;
            y += where->height - height;
            break;
        case XP_GTK_JUST_TOPLEFT:
        default:
            /* nothing to do?? */
            break;
        }
    }

    gdk_draw_layout_with_colors( DRAW_WHAT(dctx), dctx->drawGC,
                                 x, y, layout,
                                 frground, bkgrnd );
} /* draw_string_at */

static void
drawBitmapFromLBS( GtkDrawCtx* dctx, XP_Bitmap bm, XP_Rect* rect )
{
    GdkPixmap* pm;
    LinuxBMStruct* lbs = (LinuxBMStruct*)bm;
    gint x, y;
    XP_U8* bp;
    XP_U16 i;
    XP_S16 nBytes;
    XP_U16 nCols, nRows;
    
    nCols = lbs->nCols;
    nRows = lbs->nRows;
    bp = (XP_U8*)(lbs + 1);    /* point to the bitmap data */
    nBytes = lbs->nBytes;

    pm = gdk_pixmap_new( DRAW_WHAT(dctx), nCols, nRows, -1 );

    gdk_draw_rectangle( pm, dctx->drawing_area->style->white_gc, TRUE,
                        0, 0, nCols, nRows );

    x = 0;
    y = 0;

    while ( nBytes-- ) {
        XP_U8 byte = *bp++;
        for ( i = 0; i < 8; ++i ) {
            XP_Bool draw = ((byte & 0x80) != 0);
            if ( draw ) {
                gdk_draw_point( pm, dctx->drawing_area->style->black_gc, x, y );
            }
            byte <<= 1;
            if ( ++x == nCols ) {
                x = 0;
                if ( ++y == nRows ) {
                    break;
                }
            }
        }
    }

    XP_ASSERT( nBytes == -1 );   /* else we're out of sync */

    gdk_draw_drawable( DRAW_WHAT(dctx),
                       dctx->drawGC,
                       (GdkDrawable*)pm, 0, 0,
                       rect->left+2,
                       rect->top+2,
                       lbs->nCols,
                       lbs->nRows );

    g_object_unref( pm );
} /* drawBitmapFromLBS */

static void
gtk_draw_destroyCtxt( DrawCtx* p_dctx )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    GtkAllocation* alloc = &dctx->drawing_area->allocation;
    XP_U16 i;

    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawing_area->style->white_gc,
			TRUE,
			0, 0, alloc->width, alloc->height );

    for ( i = LAYOUT_BOARD; i < LAYOUT_NLAYOUTS; ++i ) {
        pango_font_description_free( dctx->fontdesc[i] );
        g_object_unref( dctx->layout[i] );
    }
    g_object_unref( dctx->pangoContext );

} /* gtk_draw_destroyCtxt */


static XP_Bool
gtk_draw_boardBegin( DrawCtx* p_dctx, DictionaryCtxt* dict, 
                     XP_Rect* rect, XP_Bool hasfocus )
{
    GdkRectangle gdkrect;
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

    gdkrect = *(GdkRectangle*)rect;
    ++gdkrect.width;
    ++gdkrect.height;
/*     gdk_gc_set_clip_rectangle( dctx->drawGC, &gdkrect ); */

    return XP_TRUE;
} /* draw_finish */

static void
gtk_draw_boardFinished( DrawCtx* p_dctx )
{
    //    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
} /* draw_finished */

static void
drawHintBorders( GtkDrawCtx* dctx, XP_Rect* rect, HintAtts hintAtts)
{
    if ( hintAtts != HINT_BORDER_NONE && hintAtts != HINT_BORDER_CENTER ) {
        XP_Rect lrect = *rect;
        insetRect( &lrect, 1 );

        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

        if ( (hintAtts & HINT_BORDER_LEFT) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left, lrect.top, 
                                0, lrect.height);
        }
        if ( (hintAtts & HINT_BORDER_TOP) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left, lrect.top, 
                                lrect.width, 0/*rectInset.height*/);
        }
        if ( (hintAtts & HINT_BORDER_RIGHT) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left+lrect.width, 
                                lrect.top, 
                                0, lrect.height);
        }
        if ( (hintAtts & HINT_BORDER_BOTTOM) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left, 
                                lrect.top+lrect.height, 
                                lrect.width, 0 );
        }
    }
}

static XP_Bool
gtk_draw_drawCell( DrawCtx* p_dctx, XP_Rect* rect, XP_UCHAR* letter, 
                   XP_Bitmap bitmap, XP_S16 owner, XWBonusType bonus, 
                   HintAtts hintAtts,
                   XP_Bool isBlank, XP_Bool highlight, XP_Bool isStar )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect rectInset = *rect;
    XP_Bool showGrid = dctx->globals->gridOn;

    eraseRect( dctx, rect );

    insetRect( &rectInset, 1 );

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

    if ( showGrid ) {
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            FALSE,
                            rect->left, rect->top, rect->width, 
                            rect->height );
    }

    /* draw the bonus colors only if we're not putting a "tile" there */
    if ( !!letter ) {
        if ( *letter == LETTER_NONE && bonus != BONUS_NONE ) {
            XP_ASSERT( bonus <= 4 );

            gdk_gc_set_foreground( dctx->drawGC, &dctx->bonusColors[bonus-1] );
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                TRUE,
                                rectInset.left, rectInset.top,
                                rectInset.width+1, rectInset.height+1 );

        } else if ( *letter != LETTER_NONE ) {
            GdkColor* foreground;

            gdk_gc_set_foreground( dctx->drawGC, &dctx->tileBack );
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                TRUE,
                                rectInset.left, rectInset.top,
                                rectInset.width+1, rectInset.height+1 );
            if ( highlight ) {
                foreground = &dctx->red;
            } else {
                foreground = &dctx->playerColors[owner];
            }

            draw_string_at( dctx, dctx->layout[LAYOUT_BOARD], letter,
                            &rectInset, XP_GTK_JUST_CENTER,
                            foreground, NULL );

            if ( isBlank ) {
                gdk_draw_arc( DRAW_WHAT(dctx), dctx->drawGC,
                              0,	/* filled */
                              rect->left, /* x */
                              rect->top, /* y */
                              rect->width,/*width, */
                              rect->height,/*width, */
                              0, 360*64 );
            }
        }
    } else if ( !!bitmap ) {
        drawBitmapFromLBS( dctx, bitmap, rect );
    }

    if ( isStar ) {
        draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], "*", 
                        rect, XP_GTK_JUST_CENTER,
                        &dctx->black, NULL );
    }

    drawHintBorders( dctx, rect, hintAtts );

    return XP_TRUE;
} /* gtk_draw_drawCell */

static void
gtk_draw_invertCell( DrawCtx* p_dctx, XP_Rect* rect )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
/*     (void)gtk_draw_drawMiniWindow( p_dctx, "f", rect); */

/*     GdkGCValues values; */

/*     gdk_gc_get_values( dctx->drawGC, &values ); */

/*     gdk_gc_set_function( dctx->drawGC, GDK_INVERT ); */

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */
/*     gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC, */
/* 			TRUE, rect->left, rect->top,  */
/* 			rect->width, rect->height ); */

/*     gdk_gc_set_function( dctx->drawGC, values.function ); */
} /* gtk_draw_invertCell */

static XP_Bool
gtk_draw_trayBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_U16 owner, 
                    XP_Bool hasfocus )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect clip = *rect;
    insetRect( &clip, -1 );
    dctx->trayOwner = owner;
/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)&clip ); */
    return XP_TRUE;
} /* gtk_draw_trayBegin */

static void
gtk_draw_drawTile( DrawCtx* p_dctx, XP_Rect* rect, XP_UCHAR* textP,
                   XP_Bitmap bitmap, XP_S16 val, XP_Bool highlighted )
{
    unsigned char numbuf[3];
    gint len; 
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect insetR = *rect;

    eraseRect( dctx, &insetR );

    if ( val >= 0 ) {
        GdkColor* foreground = &dctx->playerColors[dctx->trayOwner];
        XP_Rect formatRect = insetR;

        insetRect( &insetR, 1 );

        formatRect.left += 3;
        formatRect.width -= 6;

        gdk_gc_set_foreground( dctx->drawGC, &dctx->tileBack );
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            TRUE,
                            insetR.left, insetR.top, insetR.width, 
                            insetR.height );
        

        if ( !!textP ) {
            if ( *textP != LETTER_NONE ) { /* blank */
                draw_string_at( dctx, dctx->layout[LAYOUT_LARGE], textP,
                                &formatRect, XP_GTK_JUST_TOPLEFT,
                                foreground, NULL );

            }
        } else if ( !!bitmap ) {
            drawBitmapFromLBS( dctx, bitmap, &insetR );
        }
    
        sprintf( numbuf, "%d", val );
        len = strlen( numbuf );

        draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], numbuf, 
                        &formatRect, XP_GTK_JUST_BOTTOMRIGHT,
                        foreground, NULL );
    
        /* frame the tile */
        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            FALSE,
                            insetR.left, insetR.top, insetR.width, 
                            insetR.height );

        if ( highlighted ) {
            insetRect( &insetR, 1 );
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, insetR.left, insetR.top, 
                                insetR.width, insetR.height);
        }
    }
} /* gtk_draw_drawTile */

static void
gtk_draw_drawTileBack( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect r = *rect;

    insetRect( &r, 1 );

    gdk_gc_set_foreground( dctx->drawGC, 
                           &dctx->playerColors[dctx->trayOwner] );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
                        dctx->drawGC, TRUE, 
                        r.left, r.top, r.width, r.height );

    insetRect( &r, 1 );
    gdk_gc_set_foreground( dctx->drawGC, &dctx->tileBack );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
                        dctx->drawGC, TRUE, 
                        r.left, r.top, r.width, r.height );

    draw_string_at( dctx, dctx->layout[LAYOUT_LARGE], "?", 
                    &r, XP_GTK_JUST_CENTER,
                    &dctx->playerColors[dctx->trayOwner], NULL );
} /* gtk_draw_drawTileBack */

static void
gtk_draw_drawTrayDivider( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool selected )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect r = *rect;

    eraseRect( dctx, &r );

    ++r.left;
    r.width -= selected? 2:1;
    if ( selected ) {
	--r.height;
    }

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawGC,
			!selected, 
			r.left, r.top, r.width, r.height);
    
} /* gtk_draw_drawTrayDivider */

#if 0
static void 
gtk_draw_frameBoard( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawGC, FALSE, 
			rect->left, rect->top, rect->width, rect->height );

} /* gtk_draw_frameBoard */

static void 
gtk_draw_frameTray( DrawCtx* p_dctx, XP_Rect* rect )
{
    //    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
} /* gtk_draw_frameBoard */
#endif

static void 
gtk_draw_clearRect( DrawCtx* p_dctx, XP_Rect* rectP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect rect = *rectP;

    ++rect.width;
    ++rect.top;

    eraseRect( dctx, &rect );

} /* gtk_draw_clearRect */

static void
gtk_draw_drawBoardArrow( DrawCtx* p_dctx, XP_Rect* rectP, 
                         XWBonusType cursorBonus, XP_Bool vertical,
                         HintAtts hintAtts )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    const char* curs = vertical? "|":"-";

    draw_string_at( dctx, dctx->layout[LAYOUT_BOARD], curs,
                    rectP, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
    drawHintBorders( dctx, rectP, hintAtts );
} /* gtk_draw_drawBoardCursor */

static void
gtk_draw_scoreBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_U16 numPlayers, 
		     XP_Bool hasfocus )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */
    eraseRect( dctx, rect );
} /* gtk_draw_scoreBegin */

static void
gtkDrawDrawRemText( DrawCtx* p_dctx, XP_Rect* r, XP_U16 nTilesLeft,
                    XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char buf[10];
    XP_U16 left = r->left;
    XP_U16 top = r->top;
    XP_Bool draw = !widthP;
    
    sprintf( buf, "rem:%d", nTilesLeft );

    pango_layout_set_text( dctx->layout[LAYOUT_SMALL], buf, strlen(buf) );

    if ( draw ) {
        gdk_draw_layout_with_colors( DRAW_WHAT(dctx), dctx->drawGC,
                                     left, top, dctx->layout[LAYOUT_SMALL],
                                     &dctx->black, NULL );
    } else {
        int width, height;
        pango_layout_get_pixel_size( dctx->layout[LAYOUT_SMALL], 
                                     &width, &height );

        if ( height > HOR_SCORE_HEIGHT ) {
            height = HOR_SCORE_HEIGHT;
        }

        *widthP = width;
        *heightP = height;
    }
} /* gtkDrawDrawRemText */

static void
gtk_draw_measureRemText( DrawCtx* p_dctx, XP_Rect* r, XP_S16 nTilesLeft,
                         XP_U16* width, XP_U16* height )
{
    gtkDrawDrawRemText( p_dctx, r, nTilesLeft, width, height );
} /* gtk_draw_measureRemText */

static void
gtk_draw_drawRemText( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter,
                      XP_S16 nTilesLeft )
{
    gtkDrawDrawRemText( p_dctx, rInner, nTilesLeft, NULL, NULL );
} /* gtk_draw_drawRemText */

static void
scoreWidthAndText( GtkDrawCtx* dctx, PangoLayout* layout, char* buf, 
                   DrawScoreInfo* dsi, XP_U16* widthP, XP_U16* heightP )
{
    XP_S16 score = dsi->score;
    XP_U16 nTilesLeft = dsi->nTilesLeft;
    XP_Bool isTurn = dsi->isTurn;
    char* borders = "";
    if ( isTurn ) {
        borders = "*";
    }

    sprintf( buf, "%s%.3d", borders, score );
    if ( nTilesLeft < MAX_TRAY_TILES ) {
        char nbuf[10];
        sprintf( nbuf, ":%d", nTilesLeft );
        (void)strcat( buf, nbuf );
    }
    (void)strcat( buf, borders );

    if ( !!widthP || !!heightP ) {
        int height, width;
        pango_layout_set_text( layout, buf, strlen(buf) );
        pango_layout_get_pixel_size( layout, &width, &height );
        if ( !!widthP ) {
            *widthP = width;
        }
        if ( !!heightP ) {
            if ( height > HOR_SCORE_HEIGHT ) {
                height = HOR_SCORE_HEIGHT;
            }
            *heightP = height;
        }
    }
} /* scoreWidthAndText */

static void
gtk_draw_measureScoreText( DrawCtx* p_dctx, XP_Rect* r, 
                           DrawScoreInfo* dsi,
                           XP_U16* width, XP_U16* height )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char buf[20];
    scoreWidthAndText( dctx, dctx->layout[LAYOUT_SMALL], buf, dsi, 
                       width, height );
} /* gtk_draw_measureScoreText */

static void
gtk_draw_score_drawPlayer( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter, 
                           DrawScoreInfo* dsi )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char scoreBuf[20];
    XP_U16 x;
    XP_U16 width;

    scoreWidthAndText( dctx, dctx->layout[LAYOUT_SMALL], scoreBuf, dsi, 
                       &width, NULL );
    x = rInner->left + ((rInner->width - width) /2);

    gdk_gc_set_foreground( dctx->drawGC, &dctx->playerColors[dsi->playerNum] );

    if ( dsi->selected ) {
        gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC,
                            TRUE, rOuter->left, rOuter->top, 
                            rOuter->width, rOuter->height );
        eraseRect( dctx, rInner );
    }

    draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], scoreBuf, 
                    rInner, XP_GTK_JUST_CENTER,
                    &dctx->playerColors[dsi->playerNum], NULL );
} /* gtk_draw_score_drawPlayer */

static void
gtk_draw_score_pendingScore( DrawCtx* p_dctx, XP_Rect* rect, XP_S16 score,
                             XP_U16 playerNum )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char buf[5];
    XP_U16 left;
    XP_Rect localR;

    if ( score >= 0 ) {
	sprintf( buf, "%.3d", score );
    } else {
	strcpy( buf, "???" );
    }

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */

    localR = *rect;
    rect = &localR;
    insetRect( rect, 1 );
    eraseRect( dctx, rect );

    left = rect->left + 1;
    draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], "Pts:", 
                    rect, XP_GTK_JUST_TOPLEFT,
                    &dctx->black, NULL );
    draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], buf, 
                    rect, XP_GTK_JUST_BOTTOMRIGHT,
                    &dctx->black, NULL );
} /* gtk_draw_score_pendingScore */

static void
gtk_draw_scoreFinished( DrawCtx* p_dctx )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */

} /* gtk_draw_scoreFinished */

static void
gtkFormatTimerText( XP_UCHAR* buf, XP_S16 secondsLeft )
{
    XP_U16 minutes, seconds;

    if ( secondsLeft < 0 ) {
        *buf++ = '-';
        secondsLeft *= -1;
    }

    minutes = secondsLeft / 60;
    seconds = secondsLeft % 60;
    sprintf( buf, "% 1d:%02d", minutes, seconds );
} /* gtkFormatTimerText */

static void
gtk_draw_drawTimer( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter,
		    XP_U16 player, XP_S16 secondsLeft )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_UCHAR buf[10];

    gtkFormatTimerText( buf, secondsLeft );

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rInner ); */
    eraseRect( dctx, rInner );
    draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], buf, 
                    rInner, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
} /* gtk_draw_drawTimer */

#define MINI_LINE_HT 12
#define MINI_V_PADDING 6
#define MINI_H_PADDING 8

static unsigned char*
gtk_draw_getMiniWText( DrawCtx* p_dctx, XWMiniTextType textHint )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
    unsigned char* str;

    switch( textHint ) {
    case BONUS_DOUBLE_LETTER:
        str = "Double letter"; break;
    case BONUS_DOUBLE_WORD:
        str = "Double word"; break;
    case BONUS_TRIPLE_LETTER:
        str = "Triple letter"; break;
    case BONUS_TRIPLE_WORD:
        str = "Triple word"; break;
    case INTRADE_MW_TEXT:	
        str = "Trading tiles;\nclick D when done"; break;
    default:
        XP_ASSERT( XP_FALSE );
    }
    return str;
} /* gtk_draw_getMiniWText */

static void
gtk_draw_measureMiniWText( DrawCtx* p_dctx, unsigned char* str, 
                           XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    int height, width;

    pango_layout_set_text( dctx->layout[LAYOUT_SMALL], str, strlen(str) );
    pango_layout_get_pixel_size( dctx->layout[LAYOUT_SMALL], &width, &height );
    *heightP = height;
    *widthP = width + 6;
} /* gtk_draw_measureMiniWText */

static void
gtk_draw_drawMiniWindow( DrawCtx* p_dctx, unsigned char* text, XP_Rect* rect,
                         void** closureP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect localR = *rect;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)&localR ); */

    /* play some skanky games to get the shadow drawn under and to the
       right... */
    eraseRect( dctx, &localR );

    insetRect( &localR, 1 );
    --localR.width;
    --localR.height;
    frameRect( dctx, &localR );

    --localR.top;
    --localR.left;
    eraseRect( dctx, &localR );
    frameRect( dctx, &localR );

    draw_string_at( dctx, dctx->layout[LAYOUT_SMALL], text, 
                    &localR, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
} /* gtk_draw_drawMiniWindow */

static void
gtk_draw_eraseMiniWindow( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool lastTime,
			  void** closure, XP_Bool* invalUnder )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
    *invalUnder = XP_TRUE;
} /* gtk_draw_eraseMiniWindow */

#define SET_GDK_COLOR( c, r, g, b ) { \
     c.red = (r); \
     c.green = (g); \
     c.blue = (b); \
}
static void
draw_doNothing( DrawCtx* dctx, ... )
{
} /* draw_doNothing */

static void
allocAndSet( GdkColormap* map, GdkColor* color, unsigned short red,
             unsigned short green, unsigned short blue )

{
    gboolean success;

    color->red = red;
    color->green = green;
    color->blue = blue;

    success = gdk_colormap_alloc_color( map,
                                        color,
                                        TRUE, /* writeable */
                                        TRUE ); /* best-match */
    XP_ASSERT( success );
} /* allocAndSet */

static void
setupLayouts( GtkDrawCtx* dctx, GtkWidget* drawing_area )
{
    XP_U16 i;
    const char* fonts[] = {
/*         "Luxi Mono 12", */
        "helvetica normal 10",
        "helvetica normal 8",
        "helvetica bold 14",
    };
    PangoContext* pangoContext = gtk_widget_get_pango_context( drawing_area );
    dctx->pangoContext = pangoContext;

    for ( i = LAYOUT_BOARD; i < LAYOUT_NLAYOUTS; ++i ) {
        dctx->layout[i] = pango_layout_new( pangoContext );
        dctx->fontdesc[i] = pango_font_description_from_string( fonts[i] );
        pango_layout_set_font_description( dctx->layout[i], 
                                           dctx->fontdesc[i] ); 
    }

    pango_layout_set_alignment( dctx->layout[LAYOUT_BOARD],
                                PANGO_ALIGN_CENTER );
}

DrawCtx* 
gtkDrawCtxtMake( GtkWidget* drawing_area, GtkAppGlobals* globals )
{
    GtkDrawCtx* dctx = g_malloc( sizeof(GtkDrawCtx) );
    GdkColormap* map;

    short i;

    dctx->vtable = g_malloc( sizeof(*(((GtkDrawCtx*)dctx)->vtable)) );

    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = draw_doNothing;
    }

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, gtk );

#ifdef DRAW_WITH_PRIMITIVES
    SET_VTABLE_ENTRY( dctx->vtable, draw_setClip, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_frameRect, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertRect, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawString, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBitmap, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureText, gtk_prim );

    InitDrawDefaults( dctx->vtable );
#else

    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardFinished, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreFinished, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_eraseMiniWindow, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, gtk );
#endif

/*     SET_VTABLE_ENTRY( dctx, draw_frameBoard, gtk_ ); */
/*     SET_VTABLE_ENTRY( dctx, draw_frameTray, gtk_ ); */

    dctx->drawing_area = drawing_area;
    dctx->globals = globals;

    map = gdk_colormap_get_system();

    allocAndSet( map, &dctx->black, 0x0000, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->white, 0xFFFF, 0xFFFF, 0xFFFF );

    {
        GdkWindow *window = NULL;
        if ( GTK_WIDGET_FLAGS(GTK_WIDGET(drawing_area)) & GTK_NO_WINDOW ) {
            /* XXX I'm not sure about this function because I never used it.
             * (the name seems to indicate what you want though).
             */
            window = gtk_widget_get_parent_window( GTK_WIDGET(drawing_area) );
        } else {
            window = GTK_WIDGET(drawing_area)->window;
        }
        dctx->drawGC = gdk_gc_new( window );
    }

    map = gdk_colormap_get_system();

    allocAndSet( map, &dctx->black, 0x0000, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->white, 0xFFFF, 0xFFFF, 0xFFFF );

    allocAndSet( map, &dctx->bonusColors[0], 0xFFFF, 0xAFFF, 0xAFFF );
    allocAndSet( map, &dctx->bonusColors[1], 0xAFFF, 0xFFFF, 0xAFFF );
    allocAndSet( map, &dctx->bonusColors[2], 0xAFFF, 0xAFFF, 0xFFFF );
    allocAndSet( map, &dctx->bonusColors[3], 0xFFFF, 0xAFFF, 0xFFFF );

    allocAndSet( map, &dctx->playerColors[0], 0x0000, 0x0000, 0xAFFF );
    allocAndSet( map, &dctx->playerColors[1], 0xAFFF, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->playerColors[2], 0x0000, 0xAFFF, 0x0000 );
    allocAndSet( map, &dctx->playerColors[3], 0xAFFF, 0x0000, 0xAFFF );

    allocAndSet( map, &dctx->tileBack, 0xFFFF, 0xFFFF, 0x9999 );
    allocAndSet( map, &dctx->red, 0xFFFF, 0x0000, 0x0000 );

    setupLayouts( dctx, drawing_area );

    return (DrawCtx*)dctx;
} /* gtkDrawCtxtMake */

#endif /* PLATFORM_GTK */

