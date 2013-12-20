typedef struct {
    float x, y;
    float x_v, y_v;
    byte dir;
} spawn_t;

int sv_map_changelevel(char *filename, gamemode_t game_mode);
void spawn(entity *ent, int reset);
