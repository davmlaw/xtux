#define WEAPON_SWITCH_TIME 400

void weapon_init(void);
void weapon_fire(entity *ent);
void weapon_hit(entity *shooter, entity *victim, point P, byte weaptype);
void weapon_update(void);
void weapon_explode(entity *explosion, int radius, int damage);
void weapon_reset(entity *ent);
