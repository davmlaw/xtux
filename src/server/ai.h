void ai_move(entity *ent);
int ai_face_threat(entity *ent, int distance, int targ_type, int opposite);
void ai_fight_think(entity *ai);
void ai_shooter_think(entity *shooter);
void ai_flee_think(entity *ai);
void ai_seek_think(entity *ai);
byte calculate_dir(entity *ent, entity *target);
void ai_rotate_think(entity *ai);
