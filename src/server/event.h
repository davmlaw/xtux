typedef enum {
    EVENT_NONE,
    EVENT_MESSAGE,
    EVENT_TELEPORT,
    EVENT_ENDLEVEL,
    EVENT_DAMAGE,
    EVENT_MAP_TARGET,
    EVENT_TRIGGER,
    EVENT_SET_OBJECTIVE,
    EVENT_OBJECTIVE_COMPLETE,
    EVENT_DOOR,
    EVENT_LOSEWEAPONS,
    EVENT_LOOKAT,
    NUM_EVENTS
} event_type_t;

/* Value/Key comparison type */
typedef enum {
    VK_EQ, /* Equal to */
    VK_GT  /* Greater than */
} val_key_cmp_t;

typedef struct {
    char text[TEXTMESSAGE_STRLEN];
} event_message_data;

typedef struct {
    int x;
    int y;
    int dir;
    int speed;
} event_teleport_data;

typedef struct {
    char map[NETMSG_STRLEN];
} event_endlevel_data;

typedef struct {
    int damage;
    char map[NETMSG_STRLEN];
} event_damage_data;

typedef struct {
    int active;
    shortpoint_t target;
    char text[TEXTMESSAGE_STRLEN];
} event_map_target_data;

typedef struct {
    int letter;
    int target;
    int val;       /* Value sent to target */
    int on; /* Boolean */
    char text[TEXTMESSAGE_STRLEN];
} event_trigger_data;

typedef struct {
    char text[TEXTMESSAGE_STRLEN];
} event_set_objective_data;

typedef struct {
    char text[TEXTMESSAGE_STRLEN];
} event_objective_complete_data;

typedef struct {
    int open;
} event_door_data;

typedef struct {
    char text[TEXTMESSAGE_STRLEN];
} event_loseweapons_data;

typedef struct {
    int dir;
    char text[TEXTMESSAGE_STRLEN];
} event_lookat_data;

typedef union {
    event_message_data            message;
    event_teleport_data           teleport;
    event_endlevel_data           endlevel;
    event_damage_data             damage;
    event_map_target_data         map_target;
    event_trigger_data            trigger;
    event_set_objective_data      set_objective;
    event_objective_complete_data objective_complete;
    event_door_data               door;
    event_loseweapons_data        loseweapons;
    event_lookat_data             lookat;
} eventdata_t;

typedef struct {
    event_type_t type;
    val_key_cmp_t vk_cmp;
    int value; /* Current value, if value (vk_cmp) key, set active = 1 */
    int key;
    msec_t last_trigger;

    int active;       /* Flag for whether the event handled at all */
    int repeat;       /* If this is 0 event will be once only */
    int visible;      /* Flag for if event is drawn on the client */
    int exclusive;    /* Exclusive = only class goodie can trigger event */

    int direction; /* The way it's animating. -1 = down, 1 = up */
    int good;      /* Set to 1 when the event is set up correctly */
    entity *virtual;
    eventdata_t data;
} event_t;

void event_reset(void);
int event_setup_finish(void);
int event_trigger(entity *ent, byte id);
void read_event(const char *line);
void event_draw(client_t *cl);
void event_close(void);
