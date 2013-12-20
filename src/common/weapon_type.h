typedef enum {
    WT_NONE,
    WT_PROJECTILE,
    WT_BULLET,
    WT_ROCKET,
    WT_SLUG,
    WT_BEAM,
    NUM_WEAPON_CLASSES
} weapon_class_t;

typedef enum {
    AMMO_INFINITE,
    AMMO_BULLET,
    AMMO_SHELL,
    AMMO_ROCKET,
    AMMO_SLUG,
    AMMO_CELL,
    NUM_AMMO_TYPES
} ammo_t;


typedef struct weapon_type {
    char name[32];
    char weapon_image[32];
    char weapon_icon[32];
    char *obituary; /* String of format "%s was killed by %s" */
    weapon_class_t class; /* Used when handling physics etc */
    ammo_t ammo_type;
    short ammo_usage; /* Amount of ammo used per weapon fire */
    short initial_ammo; /* Ammo (of type ammo_type) that comes with the gun */
    byte projectile; /* projectile is an entity type */
    byte spread;
    int explosion;
    int splashdamage;
    int push_factor;
    int muzzle_flash;
    int draw_particles;
    int number;
    int max_length;
    int has_weapon_image;
    int has_weapon_icon;
    int entstop;  /* Stops on entities (default = 1) */
    int wallstop; /* Stops on walls (default = 1) */
    int damage;
    int speed;
    int frames;
    msec_t reload_time;
    struct weapon_type *next;
} weap_type_t;

/* Creates a table of ent_type_t's of size num_entities */
weap_type_t *weapon_type(byte type);
int match_weapon_type(char *name);
byte weapon_type_init(void);
ammo_t match_ammo_type(char *ammo);
