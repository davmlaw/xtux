typedef enum {
    GAME_EXIT_LEVEL,      /* Get out */
    GAME_ELIMINATE_CLASS, /* Kill off a certain class */
    GAME_STAY_ALIVE,      /* Stay alive for x seconds */
    NUM_GAME_OBJECTIVES
} game_objective_t;

typedef struct { 
    char map_name[NETMSG_STRLEN];
    char next_map_name[NETMSG_STRLEN];
    char success_msg[TEXTMESSAGE_STRLEN];
    msec_t endtime; /* Time in msecs when the level will change */
    int changelevel;
    int fraglimit;
    int timelimit; /* Minutes */
    gamemode_t mode;
    game_objective_t objective;
    int class; /* Class to kill if objective = ELIMINATE_CLASS */
    int objective_complete;
} game_t;

void game_start(void);
void game_update(void);
void game_close(void);
