typedef enum {
    PTL_BOTTOM,
    PTL_TOP
} ptl_level_t;

void particle_init(void);
void create_particles(netmsg_particles particles);
void particle_clear(void);
void particle_update(float secs);
void particles_draw(ptl_level_t level);
