/* entity.h
 * XTux Arena Server, Header file for entity related functions
 * Created 29 Jan 2000 David Lawrence
 */

typedef enum {
    CTRL_NONE, /* Items etc */
    CTRL_AI,   /* Monsters */
    CTRL_CLIENT,
    CTRL_BOT   /* AI controlled players (deathmatch) */
} controller_t;

typedef float ammunition_t; /* Ammo_t already taken in common/weapon_type.h */

typedef struct ent_struct {
    struct ent_struct *next; /* next/prev = list pointers */
    struct ent_struct *prev;
    struct ent_struct *target; /* Entities current target */
    struct client_struct *cl;
    ent_type_t *type_info;
    char *name;
    msec_t last_frame; /* Used to calculate animation */
    msec_t last_fired; /* To work out if weapon has reloaded */
    msec_t last_switch; /* Weapon switch */
    msec_t last_drip;
    msec_t last_wound;
    msec_t respawn_time; /* Respawn when this "now" >= this time  */
    float x, y;
    float x_v, y_v; /* X & Y velocity vector components */
    int speed; /* Scalar part of velocity vector */
    //int width, height;
    int health;
    int frags;
    int weapon;
    int trigger;
    int weapon_switch;
    int visible;
    int lookatdir;
    controller_t controller;
    ai_mode_t ai;
    int cliplevel;
    int dripping;
    int ai_moving;
    int id;
    int pid; /* Parent id */
    unsigned long powerup; /* Bitmask of powerups */
    msec_t powerup_expire[NUM_POWERUPS];
    ammunition_t ammo[NUM_AMMO_TYPES];
    byte *has_weapon;
    byte type;
    byte class;
    byte mode;
    byte dir; /* 0.255 degrees */

    byte color1;
    byte color2;
    byte drip1;
    byte drip2;
    
    /* Used for animation purposes */
    byte frame;
    byte img;
} entity;

/* This is called once at the start of the program to initialise entities */
void entity_init(void);
void entity_update(float secs);
/* Creates a new entity on the end of the list of type "type" and returns
   a pointer to the newly created entity */
entity *entity_new(byte type, float x, float y, float x_v, float y_v);
void entity_delete(entity *ent);
int entity_remove_all_non_clients(void);
void entity_killed(entity *shooter, entity *victim, point P, byte weaptype);
netmsg entity_to_netmsg(entity *ent);
float entity_dist(entity *ent0, entity *ent1);
entity *findent(int id);
animation_t *entity_animation(entity *ent);
void entity_spawn_effect(entity *ent);
entity *entity_alloc(void);
