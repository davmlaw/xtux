#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define BLACK BlackPixel(win.d, win.screen)

/* Dimensions of Status bar */
#define STATUS_H 58

#define DEF_WIN_W 512
#define DEF_WIN_H 384

/* Font names */
#define MED_FONT_18 "-adobe-helvetica-medium-r-*-*-18-*-*-*-*-*-*-*"
#define BOLD_FONT_14 "-*-helvetica-bold-r-*-*-14-*-*-*-*-*-iso8859-*"
#define BOLD_FONT_24 "-*-helvetica-bold-r-*-*-24-*-*-*-*-*-iso8859-*"

typedef struct {
    Display *d;
    Window w;
    Colormap cmap;
    int screen;
    Pixmap buf;        /* Pixmap which gets copied to window each frame */
    Pixmap map_buf;
    Pixmap status_buf; /* Gets copied on to win if win is dirty */
    Pixmap invisible[2]; /* a 1x1 pixmap used for invisible cursor */ 
    int width;
    int height;
    int dirty;
} win_t;

/* Window operations */
void win_init(void);
void win_close(void);
void win_update(void);
void win_set_properties(char *title, int new_w, int new_h);
void win_center_pointer(void);
void win_set_cursor(int visible);
void win_grab_pointer(void);
void win_ungrab_pointer(void);
void wait_till_expose(int timeout);

/* Graphics Routines */
void clear_area(Drawable drwbl, int x, int y, int width, int height, char *c);
void draw_pixel(Drawable dest, short x, short y, char *color);
void draw_pixels(Drawable dest, XPoint *xp, int num_points, char *color);
void draw_line(Drawable dest,int sx,int sy, int dx, int dy, int width,char *c);
void blit(Pixmap src, Drawable dest, int x, int y, int width, int height);
void trans_blit(Pixmap s, Drawable d, int x, int y, int w, int h, Pixmap msk);
void offset_blit(Pixmap, Drawable, int, int, int, int, int, int);
void trans_offset_blit(Pixmap, Drawable, int, int, int, int, int, int, Pixmap);

/* Font/text stuff */
void win_print(Drawable , char *msg, int x,int y, int font_type, char *col);
void win_center_print(Drawable dest, char *msg, int y, int fnttype, char *col);
void win_print_column(Drawable dest, char *msg, int x, int y,
		      int font_type, char *col, int width, int center,
		      int *columns);
void draw_filled_polygon(Drawable dest, XPoint *xp, int num_pts, char *color);
void draw_circle(Drawable dest, int x, int y, int diam, char *c, int solid);
int text_width(char *str, int font_type);
int font_height(int font_type);



