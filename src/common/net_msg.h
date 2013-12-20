/************************************************************
 * Network message structures sent between server & client  *
 ************************************************************/

/* NETMSG_PROTOCOL_VERSION, checked to make sure that the client and server
   use the same protocol. CHANGE THIS IF THE NET STRUCT'S CHANGE AT ALL */

#define NETMSG_PROTOCOL_VERSION 3

/*
   Ver    Date (D/M/Y)           Comment/Changes.
   ---    ------------           ----------------
    1      04/02/2001     Initial version with protocol number.
    2      04/05/2001     Killme, query_frags/frags messages added.
    3      30/01/2002     Away
*/

#define NETMSG_STRLEN 32
#define TEXTMESSAGE_STRLEN 128

enum {
    NETMSG_NONE,              /* SERVER  <---> CLIENT */
    NETMSG_NOOP,              /* SERVER  <---> CLIENT */
    NETMSG_QUERY_VERSION,     /* SERVER  <---> CLIENT */
    NETMSG_VERSION,           /* SERVER  <---> CLIENT */
    NETMSG_TEXTMESSAGE,       /* SERVER  <---> CLIENT */
    NETMSG_QUIT,              /* SERVER  <---> CLIENT */
    NETMSG_REJECTION,         /* SERVER   ---> CLIENT */
    NETMSG_SV_INFO,           /* SERVER   ---> CLIENT */
    NETMSG_CHANGELEVEL,       /* SERVER   ---> CLIENT */
    NETMSG_START_FRAME,       /* SERVER   ---> CLIENT */
    NETMSG_END_FRAME,         /* SERVER   ---> CLIENT */
    NETMSG_ENTITY,            /* SERVER   ---> CLIENT */
    NETMSG_MYENTITY,          /* SERVER   ---> CLIENT */
    NETMSG_PARTICLES,         /* SERVER   ---> CLIENT */
    NETMSG_UPDATE_STATUSBAR,  /* SERVER   ---> CLIENT */
    NETMSG_JOIN,              /* CLIENT   ---> SERVER */
    NETMSG_READY,             /* CLIENT   ---> SERVER */
    NETMSG_QUERY_SV_INFO,     /* CLIENT   ---> SERVER */
    NETMSG_CL_UPDATE,         /* CLIENT   ---> SERVER */
    NETMSG_GAMEMESSAGE,       /* SERVER   ---> CLIENT */
    NETMSG_MAP_TARGET,        /* SERVER   ---> CLIENT */
    NETMSG_OBJECTIVE,         /* SERVER   ---> CLIENT */
    NETMSG_KILLME,            /* CLIENT   ---> SERVER */
    NETMSG_QUERY_FRAGS,       /* CLIENT   ---> SERVER */
    NETMSG_FRAGS,             /* SERVER   ---> CLIENT */
    NETMSG_AWAY,              /* CLIENT   ---> SERVER */

    /* Keep this last */
    NUM_NETMESSAGES
};

/******** Message Structures ********/
typedef struct {
    byte type;
} netmsg_none;

typedef struct {
    byte type;
} netmsg_noop;

typedef struct {
    byte type;
} netmsg_query_version;

typedef struct {
    byte type;
    byte ver[3]; /* ie '1'.'0'.'0'*/
} netmsg_version;

typedef struct {
    byte type;
    char sender[NETMSG_STRLEN];
    char string[TEXTMESSAGE_STRLEN];
} netmsg_textmessage;

typedef struct {
    byte type;
} netmsg_quit;

/* SERVER --> CLIENT */
typedef struct {
    byte type;
    char reason[NETMSG_STRLEN];
} netmsg_rejection;

typedef struct {
    byte type;
    byte players;
    byte game_type;
    byte fps;
    char sv_name[NETMSG_STRLEN];
    char map_name[NETMSG_STRLEN];
} netmsg_sv_info;

typedef struct {
    byte type;
    char map_name[NETMSG_STRLEN];
} netmsg_changelevel;

typedef struct {
    byte type;
    shortpoint_t screenpos;
} netmsg_start_frame;

typedef struct {
    byte type;
} netmsg_end_frame;

typedef struct {
    byte type;
    byte entity_type;
    byte dir;
    byte mode;
    netushort x;
    netushort y;
    byte img;
    byte weapon;
} netmsg_entity;

/* myentity is the same as entity, but with a different type */

typedef struct {
    byte type;
    byte effect;
    byte dir;
    byte color1;
    byte color2;
    netushort x;
    netushort y;
    netushort length;
} netmsg_particles;

typedef struct {
    byte type;
    byte health;
    byte weapon;
    netshort frags;
    netushort ammo;
} netmsg_update_statusbar;


/* CLIENT -> SERVER */
typedef struct {
    byte type;
    byte gamemode;
    byte protocol; /* NETMSG_PROTOCOL_VERSION */
    char name[NETMSG_STRLEN];
    char map_name[NETMSG_STRLEN];
} netmsg_join;

typedef struct {
    byte type;
    byte entity_type;
    byte color1;
    byte color2;
    byte movemode;
    netushort view_w;
    netushort view_h;
} netmsg_ready;

typedef struct {
    byte type;
} netmsg_query_sv_info;

typedef struct {
    byte type;
    byte dir;
    byte keypress;  /* Bit array of key presses */
} netmsg_cl_update;


typedef struct {
    byte type;
    char string[TEXTMESSAGE_STRLEN];
} netmsg_gamemessage;

typedef struct {
    byte type;
    byte active;
    shortpoint_t target;
} netmsg_map_target;

typedef struct {
    byte type;
    char string[TEXTMESSAGE_STRLEN];
} netmsg_objective;

typedef struct {
    byte type;
} netmsg_killme;

typedef struct {
    byte type;
} netmsg_query_frags;

typedef struct {
    byte type;
    netshort frame;
    netshort frags;
    char name[NETMSG_STRLEN];
} netmsg_frags;

typedef struct {
    byte type;
} netmsg_away;

typedef union {
    /****************** KEEP THIS FIRST ****************/
    byte type; /* Lined up with the msg type's in the various struct's */
    netmsg_noop              noop;
    netmsg_query_version     query_version;
    netmsg_version           version;
    netmsg_textmessage       textmessage;
    netmsg_quit              quit;
    netmsg_rejection         rejection;
    netmsg_sv_info           sv_info;
    netmsg_changelevel       changelevel;
    netmsg_start_frame       start_frame;
    netmsg_end_frame         end_frame;
    netmsg_entity            entity;
    netmsg_particles         particles;
    netmsg_update_statusbar  update_statusbar;
    netmsg_gamemessage       gamemessage;
    netmsg_map_target        map_target;
    netmsg_objective         objective;
    netmsg_join              join;
    netmsg_ready             ready;
    netmsg_query_sv_info     query_sv_info;
    netmsg_cl_update         cl_update;
    netmsg_killme            killme;
    netmsg_query_frags       query_frags;
    netmsg_frags             frags;
    netmsg_away              away;
} netmsg;
