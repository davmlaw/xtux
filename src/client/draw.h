#include <X11/Xlib.h>
#include <X11/xpm.h>

/* Clear method for "Cool clear" */
typedef enum {
    C_HORIZONTAL,
    C_VERTICLE,
    C_BOTH
} clear_t;

typedef enum {
    BLUEPRINT,
    MINIMAP
} loadscreen_t;

void draw_load_screen(loadscreen_t type);
void draw_mini_map(Pixmap dest, char *filename, int dest_x, int dest_y,
		   int width, int height);

int tileset_load(char *tname);
void draw_map(shortpoint_t screenpos);
void draw_map_toplevel(shortpoint_t screenpos);

int cool_clear(int width, int height, int line_w, clear_t type);
void interlace(Pixmap dest, int width, int height, int spacing, char *col);
void draw_status_bar(void);
void draw_textentry(void);
void draw_explosion(short x, short y);
void draw_crosshair(netmsg_entity focus);
void draw_map_target(netmsg_entity focus);
void draw_arrow(int x, int y, int len, byte dir, char *color);
void draw_netstats(vector_t NS);
