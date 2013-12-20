#define CLIPMAP 0
#define MAP 1

/* Dimensions of a tile in pixels */
#define TILE_W 64
#define TILE_H 64

#define L_BASE      1
#define L_OBJECT    2
#define L_TOPLEVEL  4
#define L_EVENT     8
#define L_TEXT     16

typedef enum {
    BASE,
    OBJECT,
    TOPLEVEL,
    EVENT,
    TEXT,
    EVENTMETA,
    ENTITY,
    MAP_LAYERS
} maplayer_t;

enum {
    NOCLIP,
    AICLIP,
    GROUNDCLIP,
    ALLCLIP,
    NUM_CLIPLEVELS
};

struct maptext_struct {
    int x, y;
    int height, width; /* Worked out by the client using X calls */
    int font;
    char color[16];
    char *string;
    struct maptext_struct *next;
};

typedef struct maptext_struct maptext_t;

typedef struct {
    /* Map header info */
    char name[32];
    char tileset[32];
    char author[32];
    int width, height;
    int dirty; /* Something's changed and map needs to be redrawn */
    int map_type;
    /* map data, size if allocated = width * height */
    char *base;
    char *object;
    char *toplevel; /* Only used on client */
    char *event; /* Only used on server */
    /* text drawn on the map in the client */
    maptext_t *text_root;
} map_t;


map_t *map_load(char *filename, int layers, int map_type);
void map_close(map_t **map);
string_node *map_make_filename_list(void);

