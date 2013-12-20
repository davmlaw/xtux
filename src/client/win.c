#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/cursorfont.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "image.h"
#include "input.h"
#include "particle.h"

extern client_t client;
extern map_t *map;

win_t win;

char *colortab[NUM_COLORS] = {
    "white",
    "black",
    "gray",
    "navy blue",
    "blue",
    "sky blue",
    "steel blue",
    "light blue",
    "cyan",
    "dark green",
    "green",
    "khaki",
    "light yellow",
    "yellow",
    "brown",
    "orange",
    "red",
    "hot pink",
    "pink",
    "violet",
    "purple",
};

static XFontStruct  *med_18, *bold_14, *bold_24;
static GC gc, solid_gc, masked_gc, offset_gc, text_gc, bw_gc;

static void create_buffers(void);
static Window win_create(int width, int height);
static void create_gcs(void);
static void load_fonts(void);
static void create_private_colormap(void);
static int get_color(char *col);

void win_init(void)
{
    int mask = 0;
    XGCValues xgcval;

    /* Open the connection to the X-Server */
    if( (win.d = XOpenDisplay(NULL)) == NULL) {
	fprintf(stderr, "Error: Can't open display.\n");
	exit(EXIT_FAILURE);
    }

    /* Use defaults for this display /screen */
    win.screen = DefaultScreen(win.d);
    win.cmap = DefaultColormap(win.d, win.screen);

    win.w = win_create(DEF_WIN_W, DEF_WIN_H);

    /* Tell X what input events we want to receive (Kbd and exposure masks) */
    mask = KeyPressMask | KeyReleaseMask | ExposureMask;
    /* Mouse code added 21 Jan 2003 */
    mask |= PointerMotionMask | ButtonPressMask | ButtonReleaseMask;
    XSelectInput(win.d, win.w, mask);
    XSetWindowColormap(win.d, win.w, win.cmap);
    create_private_colormap(); /* For the poor folk with <8 bit displays */

    create_gcs();
    load_fonts();

    XMapRaised(win.d, win.w);

    /* Invisible cursor - 210103 */
    win.invisible[0] = XCreatePixmap(win.d, win.w, 1, 1, 1);
    win.invisible[1] = XCreatePixmap(win.d, win.w, 1, 1, 1);

    xgcval.foreground = 1;
    xgcval.background = 0; 
    bw_gc=XCreateGC(win.d,win.invisible[0],GCForeground|GCBackground,&xgcval);

    XSetForeground(win.d, bw_gc, 0);
    XDrawPoint(win.d, win.invisible[0], bw_gc, 0, 0);
    XDrawPoint(win.d, win.invisible[1], bw_gc, 0, 0);

    XFlush(win.d);

} /* Create_window */




/* Updates the actual window on the desktop. Usually it wll just copy the
   buffer straight onto the window, but if the window has been exposed
   it will redraw the status bar as well */
void win_update(void)
{

    blit(win.buf, win.w, 0, 0, client.view_w, client.view_h);

    /* Only draw status bar while playing the game */
    if( win.dirty && client.state == GAME_PLAY ) {
	if( client.debug )
	    printf("Redrawing status bar!\n");
	blit(win.status_buf, win.w, 0, client.view_h, win.width, STATUS_H);
	win.dirty = 0;
    }

    XFlush(win.d);

}


void create_buffers(void)
{
    static int buf = 0, buf_x = 0, buf_y = 0;
    static int mbuf = 0, mbuf_x = 0, mbuf_y = 0;
    static int sbuf = 0, sbuf_x = 0, sbuf_y = 0;
    static int depth = 0;

    if( depth == 0 )
        depth = DefaultDepth(win.d, win.screen);

    if( buf ) {
	/* Use old buffer if it's sufficiently large */
	if( client.view_w > buf_x || client.view_h > buf_y ) {
	    XFreePixmap(win.d, win.buf);
	    buf = 0;
	}
    }

    if( mbuf ) {
	/* Use old buffer if it's sufficiently large */
	if( client.view_w+TILE_W > mbuf_x || client.view_h+TILE_H > mbuf_y ) {
	    XFreePixmap(win.d, win.map_buf);
	    mbuf = 0;
	}
    }

    if( sbuf ) {
	/* Use old buffer if it's sufficiently large */
	if( client.view_w > sbuf_x || STATUS_H > sbuf_y ) {
	    XFreePixmap(win.d, win.status_buf);
	    sbuf = 0;
	}
    }
    
    if( buf == 0 ) {
	buf_x = client.view_w;
	buf_y = client.view_h;
	win.buf = XCreatePixmap(win.d, win.w, buf_x, buf_y, depth);
	buf = 1;
    }
    
    if( mbuf == 0 ) {
	/* Map_buf is 1 tile bigger than view_w & view_h
	   for buffering purposes */
	mbuf_x = client.view_w + TILE_W;
	mbuf_y = client.view_h + TILE_H;
	win.map_buf = XCreatePixmap(win.d, win.w, mbuf_x, mbuf_y, depth);
	mbuf = 1;
	if( map )
	    map->dirty = 1;
    }
    
    if( sbuf == 0 ) {
	sbuf_x = client.view_w;
	sbuf_y = STATUS_H;
	win.status_buf = XCreatePixmap(win.d, win.w, sbuf_x, sbuf_y,depth);
	sbuf = 1;
    }

}

/* Adjust the MAIN window's (w) properties */
void win_set_properties(char *title, int new_w, int new_h)
{
    XSizeHints size_hints;
    XWMHints wm_hints;
    XClassHint class_hints;
    image_t *icon;

    /* Update globals and buffers, as window size has changed */
    win.width = new_w;
    win.height = new_h;
    create_buffers();

    /* Set class hints (not used) */
    class_hints.res_name = NULL;
    class_hints.res_class = NULL;
    
    /* Set window to win_w * win_h (not resizable) */
    size_hints.flags = PSize | PMinSize | PMaxSize;
    size_hints.min_width = win.width;
    size_hints.max_width = win.width;
    size_hints.min_height = win.height;
    size_hints.max_height = win.height;

    /* Set icon (with mask) */
    icon = image("icon.xpm", MASKED);
    wm_hints.flags = IconPixmapHint | IconMaskHint;
    wm_hints.icon_pixmap = icon->pixmap;
    wm_hints.icon_mask = icon->mask;

    if( client.debug )
	printf("Setting window properties to %d,%d...", win.width, win.height);
    XmbSetWMProperties(win.d, win.w, title, title, 0, 0,
		       &size_hints, &wm_hints, &class_hints);  
    XResizeWindow(win.d, win.w, win.width, win.height);
    XFlush(win.d); /* Make it so */

}


void win_set_cursor(int visible)
{
    XColor c1, c2;
    Cursor cursor;

    if( visible )
	cursor = XCreateFontCursor(win.d, XC_crosshair );
    else
	cursor = XCreatePixmapCursor(win.d, win.invisible[0], win.invisible[1],
				     &c1, &c2, 0, 0);

    XDefineCursor(win.d, win.w, cursor);

}


void win_grab_pointer(void)
{
    int status, mask;

    mask = ButtonPressMask | ButtonReleaseMask;
    status = XGrabPointer(win.d, win.w, True, mask,
			  GrabModeAsync,GrabModeAsync,win.w,None,CurrentTime);

    if( status != GrabSuccess )
	printf("Error grabbing Cursor! val = %d\n", status);

}


void win_ungrab_pointer(void)
{
    XUngrabPointer(win.d, CurrentTime);
}


void win_center_pointer(void)
{

    XWarpPointer(win.d, win.w, win.w, 0, 0, 0, 0,
		 client.view_w/2, client.view_h/2);

}


void win_close(void)
{

    XCloseDisplay(win.d); /* Kill connection to the X server */

}


/* Returns when we recieve an expose event (or timeout in seconds) */
void wait_till_expose(int timeout)
{
    XEvent event;
    msec_t wait_time, t;

    t = timeout * M_SEC; /* Timeout in milliseconds */
    wait_time = 500; /* Check for new events every 500ms */

    while( t > 0 ) {
	if( XPending(win.d) ) {
	    XNextEvent(win.d, &event);
	    if( event.type == GraphicsExpose || event.type == Expose ) {
		if( client.debug )
		    printf("Got expose!\n");
		return;
	    }
	} else {
	    delay( wait_time );
	    t -= wait_time;
	}
    }

    if( client.debug )
	printf("wait_till_expose: Timed out!\n");

}


/* Clears an area of a DRAWABLE. We can't use XClearArea
   as it is only for windows */
void clear_area(Drawable drwbl, int x, int y, int width, int height, char *c)
{

    XSetForeground(win.d, solid_gc, get_color(c));
    XFillRectangle(win.d, drwbl, solid_gc, x, y, width, height);

}


void draw_pixel(Drawable dest, short x, short y, char *color)
{

    XSetForeground(win.d, solid_gc, get_color(color));
    XDrawPoint(win.d, dest, solid_gc, x, y);

}


/* Make use of X's batch drawing routines for speed */
void draw_pixels(Drawable dest, XPoint *xp, int num_points, char *color)
{

    XSetForeground(win.d, solid_gc, get_color(color));
    XDrawPoints(win.d, dest, solid_gc, xp, num_points, CoordModeOrigin);

}


/* Copies drawable src onto dest at position x,y with length width, height */
void blit(Drawable src, Drawable dest, int x, int y, int width, int height)
{
    static int o_w, o_h;
    XGCValues xgcv;

    if( width != o_w || height != o_h ) {
	xgcv.ts_x_origin = x;
	xgcv.ts_y_origin = y;
	xgcv.clip_x_origin = x;
	xgcv.clip_y_origin = y;
	XChangeGC(win.d, gc, GCClipXOrigin | GCClipYOrigin | GCTileStipXOrigin
		  | GCTileStipYOrigin, &xgcv);

	o_w = width;
	o_h = height;
    }

    XSetTile(win.d, gc, src);
    XFillRectangle(win.d, dest, gc, x, y, width, height);

}


/* Like blit, but drawn with transparency ON. This is slower than straight
   copying, so we don't do this by default */
void trans_blit(Pixmap src, Drawable dest, int x, int y, int width,
		int height, Pixmap mask)
{
    XGCValues xgcv;
    xgcv.ts_x_origin = x;
    xgcv.ts_y_origin = y;
    xgcv.clip_x_origin = x;
    xgcv.clip_y_origin = y;
    XChangeGC(win.d, masked_gc, GCClipXOrigin | GCClipYOrigin
	      | GCTileStipXOrigin | GCTileStipYOrigin, &xgcv);

    if( mask == 0 )
      printf("mask = 0\n");

    XSetClipMask(win.d, masked_gc, mask);
    XSetTile(win.d, masked_gc, src);
    XFillRectangle(win.d, dest, masked_gc, x, y, width, height);

}


/* Draws src onto dest but offset by x_o, y_o */
void offset_blit(Pixmap src, Drawable dest, int x, int y,
		 int width, int height,  int x_o, int y_o)
{
    XGCValues xgcv;

    xgcv.ts_x_origin = x_o;
    xgcv.ts_y_origin = y_o;
    xgcv.clip_x_origin = x;
    xgcv.clip_y_origin = y;
    /*
      xgcv.tile = src;
      xgcv.stipple = None;
      xgcv.clip_mask = None;
    */

    XChangeGC(win.d, offset_gc, GCClipXOrigin | GCClipYOrigin
	      | GCTileStipXOrigin | GCTileStipYOrigin, &xgcv);

    XSetTile(win.d, offset_gc, src);
    XFillRectangle(win.d, dest, offset_gc, x, y, width, height);

}


void trans_offset_blit(Pixmap src, Drawable dest, int x, int y,
		       int width, int height, int x_o, int y_o, Pixmap mask)
{
    XGCValues xgcv;

    xgcv.ts_x_origin = x_o;
    xgcv.ts_y_origin = y_o;
    xgcv.clip_x_origin = x - x_o;
    xgcv.clip_y_origin = y - y_o;

    XChangeGC(win.d, masked_gc, GCClipXOrigin | GCClipYOrigin
	      | GCTileStipXOrigin | GCTileStipYOrigin, &xgcv);
    XSetClipMask(win.d, masked_gc, mask);
    XCopyArea(win.d, src, dest, masked_gc, x_o, y_o, width, height, x, y);

}


/* Draws a text string. X draws your string so that the bottom left is
   at (x,y). This draws it from the top left. */
void win_print(Drawable dest, char *msg, int x,int y, int font_type, char *col)
{
    XGCValues xgcv;
    XFontStruct *font;
    unsigned long old_fg;
    int font_offset;

    if( font_type == 1 )
	font = med_18;
    else if( font_type == 2 )
	font = bold_14;
    else
	font = bold_24;

    font_offset = font->ascent + font->descent;
    y += font_offset;

    /* Keep a record of the old Color */
    XGetGCValues(win.d, text_gc, GCForeground, &xgcv );
    old_fg = xgcv.foreground;

    /* Set font & color for GC */
    XSetFont(win.d, text_gc, font->fid);
    XSetForeground(win.d, text_gc, get_color(col));
    XDrawString(win.d, dest, text_gc, x, y, msg, strlen(msg));
    /* Restore old foreground color */
    XSetForeground(win.d, text_gc, old_fg);
  
}


void win_center_print(Drawable dest, char *msg, int y,int font_type, char *col)
{

    win_print(dest, msg, (client.view_w - text_width(msg, font_type))/2, y,
              font_type, col);


}


#define WORD_BREAK 8 /* How many letters it will break lines at to keep words
			together */

/* Print something at x,y formatted down in a column */
void win_print_column(Drawable dest, char *msg, int x, int y,
		      int font_type, char *col, int width, int center,
		      int *rows)
{
    char *line;
    int string_length; /* Of original string */
    int i, j, k, l, len;
    int height, w, new_x, cut;

    string_length = strlen(msg);
    if( string_length < 1 || (line = malloc(string_length + 1)) == NULL ) {
	return;
    }

    height = font_height(font_type);
    i = 0;
    l = 0;
    *rows = 1; /* How many rows we print */

    while( i < string_length ) {
	len = string_length - i;
	strncpy(line, msg + i, len + 1);
	j = len;

	cut = 0;
	while( (w = text_width(line, font_type)) > width ) {
	    cut = 1;
	    line[--j] = '\0';/* Cut off one character */
	}

	/* Try and see if we could break it down to words, rather than cut
	   one in half */
	if( cut ) {
	    (*rows)++;
	    for( k = 0 ; k < WORD_BREAK && j - k > 0 ; k++ ) {
		if( line[ j - k ] == ' ' ) {
		    j -= k;
		    line[j] = '\0';
		    break;
		}
	    }
	}

	if( center )
	    new_x = (x + width - w)/2;
	else
	    new_x = x;
	win_print(dest, line, new_x, y + height * l, font_type, col);
	l++; /* Next line down */
	i += j; /* Start new line where last one finished */
    }

    free( line );

}


int text_width(char *str, int font_type)
{
    XFontStruct *font;

    if( font_type == 1 )
	font = med_18;
    else if( font_type == 2 )
	font = bold_14;
    else
	font = bold_24;

    return XTextWidth( font, str, strlen(str) );

}


int font_height(int font_type)
{
    XFontStruct *font;

    if( font_type == 1 )
	font = med_18;
    else if( font_type == 2 )
	font = bold_14;
    else
	font = bold_24;

    return font->ascent + font->descent;

}


void draw_line(Drawable dest,int sx, int sy, int dx, int dy, int w, char *c)
{

    XSetForeground(win.d, solid_gc, get_color(c));
    XSetLineAttributes(win.d, solid_gc, w, LineSolid, CapNotLast, JoinMiter);
    XDrawLine(win.d, dest, solid_gc, sx, sy, dx, dy);


}


void draw_filled_polygon(Drawable dest, XPoint *xp, int num_pts, char *c)
{

    XSetForeground(win.d, solid_gc, get_color(c));
    XFillPolygon(win.d, dest, solid_gc, xp, num_pts,Nonconvex,CoordModeOrigin);

}


void draw_circle(Drawable dest, int x, int y, int len, char *c, int solid)
{

    static int (*draw_func[2])
	(Display *,Drawable,GC,int,int,unsigned int,unsigned int,int,int) = {
	    XDrawArc,
	    XFillArc
	};

    XSetForeground(win.d, solid_gc, get_color(c));
    draw_func[!!solid](win.d, dest, solid_gc, x, y, len, len, 0, 360 * 64);

}

/* Creates a window of width * height on root window */
static Window win_create(int width, int height)
{

    return XCreateSimpleWindow(win.d, RootWindow(win.d, win.screen), 0, 0,
			       width, height, 0, BLACK, BLACK);

}


static void create_gcs(void)
{
    XGCValues xgcv;
    int vm = GCGraphicsExposures; /* Value mask */

    xgcv.graphics_exposures = False;

    /* Generic main GC */
    gc = XCreateGC(win.d, win.w, vm, &xgcv);
    XSetFillStyle(win.d, gc, FillTiled);

    masked_gc = XCreateGC(win.d, win.w, vm, &xgcv);
    XSetFillStyle(win.d, masked_gc, FillTiled);

    solid_gc = XCreateGC(win.d, win.w, vm, &xgcv);
    XSetFillStyle(win.d, solid_gc, FillSolid);

    text_gc = XCreateGC(win.d, win.w, vm, &xgcv);
    XSetForeground(win.d, text_gc, BLACK);

    offset_gc = XCreateGC(win.d, win.w, vm, &xgcv);
    XSetFillStyle(win.d, offset_gc, FillTiled);

}


static void load_fonts(void)
{

    if( !(med_18 = (XLoadQueryFont(win.d, MED_FONT_18)) )) {
	fprintf(stderr, "Error loading font: %s\n", MED_FONT_18);
	ERR_QUIT("Font error!\n", 1);
    }

    if( !(bold_14 = (XLoadQueryFont(win.d, BOLD_FONT_14)) )) {
	fprintf(stderr, "Error loading font: %s\n", BOLD_FONT_14);
	ERR_QUIT("Font error!\n", 1);
    }

    if( !(bold_24 = (XLoadQueryFont(win.d, BOLD_FONT_24)) )) {
	fprintf(stderr, "Error loading font: %s\n", BOLD_FONT_24);
	ERR_QUIT("Font error!\n", 1);
    }

}


/**** Color Routines (from Paul Coates Linux/X11 game writers page) ****/
struct {
  int pixel;
  int r, g, b;
} static idx[256];

static int get_color(char *col)
{

    XColor color;

    /* create a color from the input string - fixup by
       Tim Hentenaar.
       
       Note: when you use the same XColor for the last 2 args 
           to XAllocNamedColor() the red, green, and blue values 
           become undefined, but the pixel value is set correctly.
    */

    XAllocNamedColor(win.d,win.cmap,col,&color,&color);
    return color.pixel;

#if 0

	int i, cindx;
	double rd, gd, bd, dist, mindist;
	XColor color;

	/* create a color from the input string */
	XParseColor(win.d, win.cmap, col, &color);

	/* find closest match */
	cindx = -1;
	mindist = 196608.0;             /* 256.0 * 256.0 * 3.0 */
	for (i=0; i<256; i++) {
		rd = (idx[i].r - color.red) / 256.0;
		gd = (idx[i].g - color.green) / 256.0;
		bd = (idx[i].b - color.blue) / 256.0;
		dist = (rd * rd) + (gd * gd) + (bd * bd);
		if (dist < mindist) {
			mindist = dist;
			cindx = idx[i].pixel;
			if (dist == 0.0) break;
		}
	}

	return cindx;

#endif

}


static void alloc_color(int i, int r, int g, int b)
{

    XColor col;
    
    col.red = r;
    col.green = g;
    col.blue = b;
    col.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(win.d, win.cmap, &col)) {
	idx[i].pixel = col.pixel;
    } else {
	if (win.cmap == DefaultColormap(win.d, win.screen)) {
	    win.cmap = XCopyColormapAndFree(win.d, win.cmap);
	    XSetWindowColormap(win.d, win.w, win.cmap);
	    col.red =	r; col.green = g; col.blue = b;
	    col.flags = DoRed | DoGreen | DoBlue;
	    if (XAllocColor(win.d, win.cmap, &col)) {
		idx[i].pixel = col.pixel;
	    }
	}

    }

}


static void create_private_colormap()
{
    int i, j, k;
    int count;

    for (i=0; i<256; i++)
	idx[i].pixel = 0;
    
    alloc_color(0, 0, 0, 0);
    alloc_color(1, 0, 0, 0);
    alloc_color(2, 0, 0, 0);
    count = 3;

    for (i=0; i<5; i++)
	for (j=0; j<5; j++)
	    for (k=0; k<5; k++) {
		idx[count].r = 65535 - (i*16384);
		if(idx[count].r<0)
		    idx[count].r=0;
		idx[count].g = 65535 - (j*16384);
		if(idx[count].g<0)
		    idx[count].g=0;
		idx[count].b = 65535 - (k*16384); 
		if(idx[count].b<0)
		    idx[count].b=0;
		alloc_color(count, idx[count].r, idx[count].g, idx[count].b);
		count++;
	    }
    
    for (i=0; i<4; i++)
	for (j=0; j<4; j++)
	    for (k=0; k<4; k++) {
		idx[count].r = 60415 - (i*16384);
		if(idx[count].r<0)
		    idx[count].r=0;
		idx[count].g = 60415 - (j*16384);
		if(idx[count].g<0)
		    idx[count].g=0;
		idx[count].b = 60415 - (k*16384);
		if(idx[count].b<0)
		    idx[count].b=0;
		alloc_color(count, idx[count].r, idx[count].g, idx[count].b);
		count++;
		idx[count].r = 55295 - (i*16384);
		if(idx[count].r<0)
		    idx[count].r=0;
		idx[count].g = 55295 - (j*16384);
		if(idx[count].g<0)
		    idx[count].g=0;
		idx[count].b = 55295 - (k*16384);
		if(idx[count].b<0)
		    idx[count].b=0;
		alloc_color(count, idx[count].r, idx[count].g, idx[count].b);
		count++;
	    }
}
