#define DEFAULT_MAXCLIENTS 32

typedef struct client_struct {
    struct client_struct *next;
    struct client_struct *prev;
    netconnection_t *nc;
    char name[NETMSG_STRLEN];
    entity *ent;
    shortpoint_t screenpos;
    int bad;
    int id;
    int team; /* Not yet implemented */
    movemode_t movemode;
    sv_status_t status;
    short view_w, view_h;
    netshort frags;
    short ammo;
    byte health;
    byte weapon;
    byte keypress;
} client_t;


