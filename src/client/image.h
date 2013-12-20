#include <X11/Xlib.h>
#include <X11/xpm.h>

#define NOMASK 0
#define MASKED 1

typedef struct imgptr {
    struct imgptr *next;
    char *name;
    int has_mask; /* Image was loaded with an ATTEMPT to get a mask */
    Pixmap pixmap;
    Pixmap mask;
} image_t;

image_t *image(char *name, int has_mask);
void screenshot(void);
int image_close(void);
