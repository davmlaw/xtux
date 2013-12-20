#include <sys/types.h>

/* Some libraries on some systems do not define types we need. */

#if 1 /* FIXME: What do I check here? Posix/ISO numbers? */
typedef u_int8_t byte;
typedef u_int16_t netushort;
typedef int16_t netshort;
#else
typedef unsigned char byte;
typedef ushort netushort;
typedef short netshort;
#endif

#if 0 /* Some older libs do not define this */
typedef unsigned int socklen_t;
#endif

#define DEFAULT_PORT 8390 /* the port users will be sending to */
#define DEFAULT_TILESET "default.table"
#define DEGREES 256 /* Degrees in our circle */

/* Maps have max dimensions of 1024 * 1024, as the position in pixels is
   stored in a short (16 bits), which would overflow at 1024 * 64 (TILE_W),
   so the longest line of input in a map could be 1024 bytes */
#define MAXLINE 1024

/* Used for flags in clients keypress */
#define FORWARD_BIT 1<<0
#define BACK_BIT    1<<1
#define LEFT_BIT    1<<2
#define RIGHT_BIT   1<<3
#define B1_BIT      1<<4
#define B2_BIT      1<<5
#define WEAP_UP_BIT 1<<6
#define WEAP_DN_BIT 1<<7

typedef enum {
    SAVETHEWORLD,
    HOLYWAR,
    NUM_GAME_MODES
} gamemode_t;

typedef struct {
    int x;
    int y;
} point;

typedef struct {
    netushort x;
    netushort y;
} shortpoint_t;

typedef struct {
    float x;
    float y;
} vector_t;

typedef enum {
  NORMAL,
  CLASSIC
} movemode_t;


/* Just a general node for use in doubly linked string lists */
typedef struct str_struct {
    char *string;
    struct str_struct *next;
    struct str_struct *prev;
} string_node;

enum {
    X,
    Y,
    NUM_DIMENSIONS
};

/* States a client can be on the server */
typedef enum {
    QUITTING,
    CONNECTING,
    JOINING,
    AWAY,
    ACTIVE,
    OBSERVER
} sv_status_t;

/* Particle effect types */
enum {
    P_RAILSLUG,
    P_EXPLOSION,
    P_BLOOD,
    P_SHARDS,
    P_BEAM,
    P_MFLASH,
    P_DRIP,
    P_SPAWN,
    P_TELEPORT,
    NUM_EFFECTS
};

/* CHOP is faster than chomp, but doesn't check to see if the character
   it is chopping is a '\n' or not */
#define CHOP(a) a[ strlen(a) - 1 ] = '\0'
#define CHOMP(a) chomp(a)
#define DFNTOSTR(a) #a
#define MAX(a,b) (a > b? a : b)
#define MIN(a,b) (a < b? a : b)
#define ERR_QUIT(a,b) err_quit(a, b, VERSION, __FILE__, __LINE__)

#include "common.h"
#include "maths.h"
#include "net.h"
#include "timing.h"
#include "weapon_type.h"
#include "entity_type.h"
#include "map.h"
#include "colors.h"
#include "datafile.h"
