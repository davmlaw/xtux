enum {
    GOODIE,
    BADDIE,
    NEUTRAL,
    ITEM,
    PROJECTILE,
    VIRTUAL,
    NUM_ENT_CLASSES
};


/* Powerups */
typedef enum {
    PU_RANDOM,
    PU_HALF_AMMO_USAGE,
    PU_DOUBLE_FIRE_RATE,
    PU_WOUND,
    PU_RESISTANCE,
    PU_FROZEN,
    PU_INVISIBILITY,
    PU_E,
    PU_FIXED_WEAPON,
    PU_SWITCHEROO,
    NUM_POWERUPS
} powerup_t;

/* Things items can do to the thing that picked them up */
typedef enum {
    HEAL,
    FULLHEAL,
    GIVEWEAPON,
    GIVEAMMO,
    GIVEKEY,
    POWERUP,
    NUM_ITEM_ACTIONS
} item_action_t;

typedef enum {
    LIMBO, /* Taken, but to be respawned */
    DEAD,
    DYING,
    GIBBED,
    FROZEN,
    ALIVE,
    PATROL,
    FIDGETING, /* Animating */
    SLEEP,
    ATTACK,
    ENLIGHTENED,
    COMEDOWN,
    NUM_ENT_MODES
} ent_mode_t;

typedef enum {
    AI_NONE,
    AI_FIGHT,
    AI_FLEE,
    AI_SEEK,
    AI_ROTATE,
    NUM_AI_MODES
} ai_mode_t;

typedef struct {
    char pixmap[32];
    msec_t framelen;
    int stationary_ani;
    int draw_hand;
    int img_w;
    int img_h;
    int arm_x;
    int arm_y;
    byte order[4];
    byte images;
    byte directions;
    byte frames;
} animation_t;

typedef struct entity_type {
    struct entity_type *next;
    char *line;
    int health;
    int touchdamage;
    int explosive;
    int speed;
    int accel;
    int width;
    int height;
    int draw_weapon;
    int bleeder;
    int cliplevel;
    ent_mode_t mode;
    char name[32];
    /* weapon_name stores the string of the weapon that is either the entities
       default weapon, OR the weapon the entity gives you if it is a giveweapon
       item. After the weapon config file is read, the string is matched
       against found weapons and .weapon or .item_type is set accordingly */
    char weapon_name[16];
    byte weapon;
    byte class;

    /* Item stuff */
    int item_value;
    int item_type;
    msec_t item_respawn_time;
    item_action_t item_action;
    powerup_t item_powerup;
    char *item_pickup_str;   /* Told to the client who's entity picked it up */
    char *item_announce_str; /* Told to everyone upon item pickup */

    /* AI stuff */
    ai_mode_t ai;
    int sight; /* How far the enemy can see (used in AI) */

    /* Dripping stuff */
    int dripping;
    msec_t drip_time;
    byte drip1, drip2; /* Default dripping colors */
    
    /* Animation details */
    animation_t *animation[NUM_ENT_MODES];
} ent_type_t;

byte entity_types(void);
/* Creates a table of ent_type_t's of size num_entities */
ent_type_t *entity_type(byte type);
animation_t *entity_type_animation(ent_type_t *et, int mode);
int match_entity_type(char *name);
byte entity_type_init(void);
