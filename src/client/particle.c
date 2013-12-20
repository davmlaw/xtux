/*
  Particles are grouped together in colours, because that way we can send a
  array of all of the colours for a particular colour in one X call, which
  is significantly faster than doing lots of individual draw pixel calls.
*/

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "particle.h"
#define MAXPOINTS 100000

extern client_t client;
extern win_t win;
extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];

typedef struct ptl_struct {
    msec_t expire; /* Expires after this time */
    unsigned int num_ptl;
    ptl_level_t level;
    int stationary; /* when set no movement will be calculated, saves cycles */
    /* Both of these will be of size "num_ptl" */
    vector_t *pos;
    vector_t *vel;
    /* list pointers */
    struct ptl_struct *prev;
    struct ptl_struct *next;
} particles_t;

typedef struct {
    byte color;
    particles_t *root;
    particles_t *tail;
} ptl_tab_t;

static ptl_tab_t ptl_tab[NUM_COLORS];
static XPoint *xp;

static particles_t *particles_new(int col, ptl_level_t level, unsigned int n);
static void particles_delete(int col, particles_t *ptl);

void particle_init(void)
{

    memset(ptl_tab, 0, NUM_COLORS * sizeof(ptl_tab_t));
    if( (xp = malloc( MAXPOINTS * sizeof(XPoint) )) == NULL ) {
	perror("malloc");
	printf("Failed to allocate %d particles!\n", MAXPOINTS);
	ERR_QUIT("Error allocating particles", -1);
    }
    
}

/* Create a new particles structure adding it to table entry color
   and having num particles */
static particles_t *particles_new(int color, ptl_level_t level, unsigned int n)
{
    particles_t *ptl;
    vector_t *pos;
    vector_t *vel;

    /* Allocate the particle structure */
    if( (ptl = (particles_t *)malloc(sizeof(particles_t))) == NULL ) {
	perror("Malloc");
	ERR_QUIT("Couldn't create particles strucure!", n);
    }

    if( (pos = (vector_t *)malloc(n * sizeof(vector_t))) == NULL ) {
	perror("Malloc");
	fprintf(stderr, "Error allocating %d \n", n);
	ERR_QUIT("Failed to allocate particles position array", n);    
    }

    if( (vel = (vector_t *)malloc(n * sizeof(vector_t))) == NULL ) {
	perror("Malloc");
	ERR_QUIT("Failed to allocate particles velocity array", n);
    }

    ptl->level = level;
    ptl->num_ptl = n;
    ptl->pos = pos;
    ptl->vel = vel;
    ptl->stationary = 0; /* Default to calculate movement */

    /* Add it to the list */
    if( ptl_tab[color].root == NULL )
	ptl_tab[color].root = ptl;

    if( ptl_tab[color].tail != NULL )
	ptl_tab[color].tail->next = ptl;
    ptl->prev = ptl_tab[color].tail;
    ptl->next = NULL;

    ptl_tab[color].tail = ptl;

    return ptl;

}


void particles_delete(int color, particles_t *ptl)
{

    /* Make sure that root & Tail will still point at the
       right spot after ptl is deleted */
    if( ptl_tab[color].root == ptl )
	ptl_tab[color].root = ptl->next;
    if( ptl_tab[color].tail == ptl )
	ptl_tab[color].tail = ptl->prev;

    if( ptl->prev )
	ptl->prev->next = ptl->next;

    if( ptl->next )
	ptl->next->prev = ptl->prev;

    free(ptl->pos);
    free(ptl->vel);
    free(ptl);

}


#define RAIL_TIME 1 /* Seconds rail trail stays up */
/* OP = Outer particles */
#define OP_SPACING 2
#define OP_FREQ 35
#define OP_RADIUS 4.0
 /* Outer Partcle Expansion Velocity MAXIMUM speed. Actual particles will
    be assigned a percentage of this value */
#define OP_VEL 0.9

/* IP = Inner Particles */
#define IP_SPACING 3
#define IP_DEV 1 /* Deviation from straight line */

void railslug_init(byte dir, netushort src_x, netushort src_y,
		   netushort length, byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    unsigned int num_ip, num_op;    /* Number of Inner and Outer Particles */
    int num_ptl, i;
    float x_d, y_d;
    byte p;

    /* Direction Vectors */
    x_d = sin_lookup[ dir ];
    y_d = -cos_lookup[ dir ];

    /* Number of Inner particles */
    num_ip = length / IP_SPACING;
    if( num_ip > MAXPOINTS )
	num_ip = MAXPOINTS;
    /* Number of outer particles */
    num_op = (length * OP_FREQ)/ OP_SPACING;
    if( num_op > MAXPOINTS )
	num_op = MAXPOINTS;

    expire = gettime() + M_SEC * RAIL_TIME;

    /* Create the INNER particles */
    ptl = particles_new(color1, PTL_TOP, num_ip);
    ptl->expire = expire;
    for( i=0 ; i < num_ip ; i++ ) {
	ptl->pos[i].x = src_x + x_d * i * IP_SPACING;
	ptl->pos[i].y = src_y + y_d * i * IP_SPACING;
	/* Add some randomness to inner particle trail */
	ptl->pos[i].x += rand()%(2*IP_DEV)-IP_DEV;
	ptl->pos[i].y += rand()%(2*IP_DEV)-IP_DEV;

	/* Inner particles don't move */
	ptl->stationary = 1;
    }

    /* Create the OUTER particles */
    ptl = particles_new(color2, PTL_TOP, num_op);
    num_ptl = ptl->num_ptl;
    ptl->expire = expire;

    for( i=0 ; i < num_ptl ; i ++ ) {
	/* Overflow to get a direction (0..255) */
	p = i;
	
	ptl->pos[i].x = src_x + x_d * OP_SPACING * (float)i/OP_FREQ;
	ptl->pos[i].y = src_y + y_d * OP_SPACING * (float)i/OP_FREQ;

	ptl->vel[i].x = sin_lookup[ p ] * OP_RADIUS;
	ptl->vel[i].y = cos_lookup[ p ] * OP_RADIUS;

	ptl->pos[i].x += ptl->vel[i].x;
	ptl->pos[i].y += ptl->vel[i].y;

	/*
	  Particle velocities are assigned a percentage of OP_VEL depending
	  on their position in the particle list ranging from 100% at i=0
	  to 0% where i = num_ptl - 1
	*/
	ptl->vel[i].x *= OP_VEL * (num_ptl - i)/ num_ptl;
	ptl->vel[i].y *= OP_VEL * (num_ptl - i)/ num_ptl;

    }


}


#define BEAM_FREQ 8
#define BEAM_SPACING 2
#define BEAM_AMP 3.5
#define BEAM_COLORS 2
void beam_init(byte dir, netushort src_x, netushort src_y, netushort length,
	       byte color1, byte color2)
{
    particles_t *ptl;
    msec_t now;
    unsigned int num;
    int i, beam;
    float x_d, y_d;
    byte beamcolors[BEAM_COLORS] = { COL_SKYBLUE, COL_LBLUE };
    byte p, start, color;

    now = gettime();

    /* Direction Vectors */
    x_d = sin_lookup[ dir ];
    y_d = -cos_lookup[ dir ];

    num = length * BEAM_FREQ / BEAM_SPACING;
    if( num > MAXPOINTS )
	num = MAXPOINTS;

    for( beam = 0 ; beam < 3 ; beam++ ) {
	/* Start = starting position in wave */
	start = rand()%(DEGREES/4) + DEGREES/(beam + 1);
	color = beamcolors[ beam%BEAM_COLORS ];
	ptl = particles_new(color, PTL_TOP, num);
	ptl->expire = now; /* expire Instantly */
	ptl->stationary = 1;

	for( i=0 ; i < num ; i ++ ) {
	    p = i + start;
	    
	    ptl->pos[i].x = src_x + x_d * BEAM_SPACING * (float)i/BEAM_FREQ;
	    ptl->pos[i].y = src_y + y_d * BEAM_SPACING * (float)i/BEAM_FREQ;
	    
	    ptl->pos[i].x += sin_lookup[ p ] * BEAM_AMP;
	    ptl->pos[i].y -= cos_lookup[ p ] * BEAM_AMP;
	}
    }

}


#define MIN_VEL 2.0
#define MAX_VEL 100.0

#define INNER COL_RED
#define OUTER COL_ORANGE
void explosion_init(byte dir, netushort src_x, netushort src_y,
		    netushort length, byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    float vel, f, inc;
    int i;

    expire = gettime() + M_SEC;

    /* Outer particles */
    ptl = particles_new(INNER, PTL_TOP, length);
    ptl->expire = expire;
	
    f = 0.0;
    inc = (float)length / DEGREES;
    for( i = 0 ; i < length ; i++ ) {
	ptl->pos[i].x = src_x;
	ptl->pos[i].y = src_y;
	
	f += inc;
	dir = f + 0.5;
	vel = MIN_VEL + (MAX_VEL*rand()/(RAND_MAX+MIN_VEL));
	ptl->vel[i].x = vel * sin_lookup[dir];
	ptl->vel[i].y = vel * -cos_lookup[dir];
    }

    length = length * 1.5;

    /* Inner particles */
    ptl = particles_new(OUTER, PTL_TOP, length);
    ptl->expire = expire;
	
    f = 0.0;
    for( i = 0 ; i < length ; i++ ) {
	ptl->pos[i].x = src_x;
	ptl->pos[i].y = src_y;
	
	f += inc;
	dir = f + 0.5;
	vel = MIN_VEL + (MAX_VEL*rand()/(RAND_MAX+MIN_VEL));
	vel *= 0.8;
	ptl->vel[i].x = vel * sin_lookup[dir];
	ptl->vel[i].y = vel * -cos_lookup[dir];
    }


}


#define SHARD_ANGLE 128
#define SHARD_EXP_VEL 10
#define SHARD_MAX_VEL 20
#define SHARD_MIN_VEL 0.5
void shards_init(byte dir, netushort src_x, netushort src_y, netushort length,
		 byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    unsigned int num_sh1, num_sh2; /* 2 shards colors */
    int i;
    float vel;
    byte d;

    num_sh1 = 8; /* Yellow */
    num_sh2 = 128; /* Gray */

    expire = gettime() + M_SEC * 0.6;

    ptl = particles_new(color1, PTL_TOP, num_sh1);
    ptl->expire = expire;
    for( i=0 ; i < num_sh1 ; i++ ) {
	ptl->pos[i].x = src_x;
	ptl->pos[i].y = src_y;
	d = dir + 128;
	d += i%SHARD_ANGLE - SHARD_ANGLE/2;
	ptl->vel[i].x = sin_lookup[dir + 64 + rand()%64] * SHARD_EXP_VEL;
	ptl->vel[i].y = -cos_lookup[dir + 64 + rand()%64] * SHARD_EXP_VEL;
	vel = SHARD_MIN_VEL + (SHARD_MAX_VEL*rand()/(RAND_MAX+SHARD_MIN_VEL));
	ptl->vel[i].x += vel * sin_lookup[d] * 10;
	ptl->vel[i].y += vel * -cos_lookup[d] * 10;
    }

    ptl = particles_new(color2, PTL_TOP, num_sh2);
    ptl->expire = expire;
    for( i=0 ; i < num_sh2 ; i++ ) {
	ptl->pos[i].x = src_x;
	ptl->pos[i].y = src_y;
	d = dir + 128;
	d += i%SHARD_ANGLE - SHARD_ANGLE/2;
	ptl->vel[i].x = sin_lookup[rand()%256] * SHARD_EXP_VEL;
	ptl->vel[i].y = -cos_lookup[rand()%256] * SHARD_EXP_VEL;
	vel = SHARD_MIN_VEL + (SHARD_MAX_VEL*rand()/(RAND_MAX+SHARD_MIN_VEL));
	ptl->vel[i].x += vel * sin_lookup[d] * 20;
	ptl->vel[i].y += vel * -cos_lookup[d] * 20;
    }

    /* "Extra" particles (ie bullet holes) */
#define NUM_HOLEPTLS 16
    if( client.ep_expire ) {
	if( color2 == COL_GRAY )
	    color2 = COL_BLACK;

	ptl = particles_new(color2, PTL_BOTTOM, NUM_HOLEPTLS);
	if( client.ep_expire < 0 )
	    ptl->expire = ULONG_MAX; /* Should never expire */
	else
	    ptl->expire = gettime() + M_SEC * client.ep_expire;
	ptl->stationary = 1; /* Zero velocity */
	for( i=0 ; i < NUM_HOLEPTLS ; i++ ) {
	    ptl->pos[i].x = src_x + (rand()%6) - 3; /* +/- 3 */
	    ptl->pos[i].y = src_y + (rand()%6) - 3; /* +/- 3 */
	}
    }

}


#define BLOOD_ANGLE 8 /* deviance from dir */
#define BLOOD_TIME 1.0
#define BLOOD_EXP_VEL 10 /* Amount blood explodes outwards */
#define BLOOD_MAX_VEL 20.0
#define BLOOD_MIN_VEL 0.5
void blood_init(byte dir, netushort src_x, netushort src_y, netushort num,
		byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    float vel;
    unsigned int i, num_particles;
    byte d;

    /* Draw red mini explosion (using shards function) as well */
    shards_init(dir, src_x, src_y, num * 2, COL_RED, COL_RED);

    expire = gettime() + M_SEC * BLOOD_TIME;
    num_particles = num * 30;

    ptl = particles_new(color1, PTL_TOP, num_particles);
    ptl->expire = expire;
    for( i=0 ; i < num_particles ; i++ ) {
	ptl->pos[i].x = src_x;
	ptl->pos[i].y = src_y;
	d = dir + i%BLOOD_ANGLE - BLOOD_ANGLE/2;
	ptl->vel[i].x = sin_lookup[i%DEGREES] * BLOOD_EXP_VEL;
	ptl->vel[i].y = -cos_lookup[i%DEGREES] * BLOOD_EXP_VEL;
	vel = BLOOD_MIN_VEL + (BLOOD_MAX_VEL*rand()/(RAND_MAX+BLOOD_MIN_VEL));
	ptl->vel[i].x += vel * sin_lookup[d] * 16;
	ptl->vel[i].y += vel * -cos_lookup[d] * 16;
    }

}


/* Muzzle flash effect for firing weapons */
#define MAX_OUTER_FLAME 32
#define MUZZLE_OUTER_SPREAD 5

#define MAX_INNER_FLAME 20
#define MUZZLE_INNER_SPREAD 3

void mflash_init(byte dir, netushort src_x, netushort src_y, netushort num,
		 byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    float x_d, y_d;
    unsigned int i, num_particles;
    int s, t;
    byte d;

    d = dir + 64;
    x_d = sin_lookup[d];
    y_d = -cos_lookup[d];

    expire = gettime();

    /* Outer flame */
    num_particles = 300;
    ptl = particles_new(color1, PTL_TOP, num_particles);
    ptl->expire = expire;
    ptl->stationary = 1;

    for( i=0 ; i < num_particles ; i++ ) {
	s = rand()%MUZZLE_OUTER_SPREAD;
	if( s && i & 1 ) /* Odd */
	    t = s * -1;
	else
	    t = s;

	ptl->pos[i].x = src_x + x_d * t;
	ptl->pos[i].y = src_y + y_d * t;

	s = s * MAX_OUTER_FLAME/MUZZLE_OUTER_SPREAD;
	s = rand()%(MAX_OUTER_FLAME - s);
	ptl->pos[i].x += sin_lookup[dir] * s;
	ptl->pos[i].y -= cos_lookup[dir] * s;
    }

    /* Inner flame */
    num_particles = 30;
    ptl = particles_new(color2, PTL_TOP, num_particles);
    ptl->expire = expire;
    ptl->stationary = 1;

    for( i=0 ; i < num_particles ; i++ ) {
	s = rand()%MUZZLE_INNER_SPREAD;
	if( s && i & 1 ) /* Odd */
	    t = s * -1;
	else
	    t = s;

	ptl->pos[i].x = src_x + x_d * t;
	ptl->pos[i].y = src_y + y_d * t;

	s = s * MAX_INNER_FLAME/MUZZLE_INNER_SPREAD;
	s = rand()%(MAX_INNER_FLAME - s);
	ptl->pos[i].x += sin_lookup[dir] * s;
	ptl->pos[i].y -= cos_lookup[dir] * s;
    }

}


#define DRIP_TIME 1500
#define DRIP_AMOUNT 400
#define DRIP_VEL 18.0
#define DRIP_RAD 4
void drip_init(byte dir, netushort src_x, netushort src_y, netushort num,
		byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    unsigned int i, num_particles;
    float f, inc, vel, rad;

    expire = gettime() + DRIP_TIME;

    num_particles = DRIP_AMOUNT;
    ptl = particles_new(color1, PTL_TOP, num_particles);
    ptl->expire = expire;

    f = 0.0;
    inc = (float)num_particles / DEGREES;
    for( i = 0 ; i < num_particles ; i++ ) {
	f += inc;
	dir = f + 0.5;
	rad = rand()%DRIP_RAD;
	ptl->pos[i].x = src_x + rad * sin_lookup[dir];
	ptl->pos[i].y = src_y - rad * cos_lookup[dir];
	vel = 2 * (DRIP_VEL*rand()/RAND_MAX) - DRIP_VEL;
	ptl->vel[i].x = sin_lookup[dir] * vel;
	ptl->vel[i].y = cos_lookup[dir] * vel;
    }

    num_particles = 100;
    ptl = particles_new(color2, PTL_TOP, num_particles);
    ptl->expire = expire;

    f = 0.0;
    inc = (float)num_particles / DEGREES;
    for( i = 0 ; i < num_particles ; i++ ) {
	f += inc;
	dir = f + 0.5;
	rad = rand()%DRIP_RAD;
	ptl->pos[i].x = src_x + rad * sin_lookup[dir];
	ptl->pos[i].y = src_y - rad * cos_lookup[dir];
	vel = 2 * (DRIP_VEL*rand()/RAND_MAX) - DRIP_VEL;
	ptl->vel[i].x = sin_lookup[dir] * vel;
	ptl->vel[i].y = cos_lookup[dir] * vel;
    }

}


#define SPAWN_TIME 750
#define SPAWN_VEL 1.0
void spawn_init(byte dir, netushort src_x, netushort src_y, netushort num,
		byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    unsigned int i, num_particles;
    float f, inc, vel, rad;

    expire = gettime() + SPAWN_TIME;

    num_particles = 1000;
    ptl = particles_new(color1, PTL_TOP, num_particles);
    ptl->expire = expire;

    f = 0.0;
    inc = (float)num_particles / DEGREES;
    for( i = 0 ; i < num_particles ; i++ ) {
	f += inc;
	dir = f + 0.5;
	rad = rand()%num;
	ptl->pos[i].x = src_x + rad * sin_lookup[dir];
	ptl->pos[i].y = src_y - rad * cos_lookup[dir];
	vel = 2 * (SPAWN_VEL*rand()/RAND_MAX) - SPAWN_VEL;
	ptl->vel[i].x = sin_lookup[dir] * vel;
	ptl->vel[i].y = cos_lookup[dir] * vel;
    }

    num_particles = 400;
    ptl = particles_new(color2, PTL_TOP, num_particles);
    ptl->expire = expire;

    f = 0.0;
    inc = (float)num_particles / DEGREES;
    for( i = 0 ; i < num_particles ; i++ ) {
	f += inc;
	dir = f + 0.5;
	rad = rand()%num;
	ptl->pos[i].x = src_x + rad * sin_lookup[dir];
	ptl->pos[i].y = src_y - rad * cos_lookup[dir];
	vel = 2 * (SPAWN_VEL*rand()/RAND_MAX) - SPAWN_VEL;
	ptl->vel[i].x = sin_lookup[dir] * vel;
	ptl->vel[i].y = cos_lookup[dir] * vel;
    }

}


#define TELEPORT_TIME 0
#define TELEPORT_VEL 1.3
void teleport_init(byte dir, netushort src_x, netushort src_y, netushort num,
		byte color1, byte color2)
{
    particles_t *ptl;
    msec_t expire;
    unsigned int i, num_particles;
    float f, inc, vel, rad;

    expire = gettime() + TELEPORT_TIME;

    num_particles = 700;
    ptl = particles_new(color1, PTL_TOP, num_particles);
    ptl->expire = expire;

    f = 0.0;
    inc = (float)num_particles / DEGREES;
    for( i = 0 ; i < num_particles ; i++ ) {
	f += inc;
	dir = f + 0.5;
	rad = rand()%num;
	ptl->pos[i].x = src_x + rad * sin_lookup[dir];
	ptl->pos[i].y = src_y - rad * cos_lookup[dir];
	vel = 2 * (TELEPORT_VEL*rand()/RAND_MAX) - TELEPORT_VEL;
    }

    num_particles = 250;
    ptl = particles_new(color2, PTL_TOP, num_particles);
    ptl->expire = expire;

    f = 0.0;
    inc = (float)num_particles / DEGREES;
    for( i = 0 ; i < num_particles ; i++ ) {
	f += inc;
	dir = f + 0.5;
	rad = rand()%num;
	ptl->pos[i].x = src_x + rad * sin_lookup[dir];
	ptl->pos[i].y = src_y - rad * cos_lookup[dir];
	vel = 2 * (TELEPORT_VEL*rand()/RAND_MAX) - TELEPORT_VEL;
    }

}


void create_particles(netmsg_particles p)
{
    static void (*effect[NUM_EFFECTS])
	(byte, netushort, netushort, netushort, byte, byte) = {
	    railslug_init,
	    explosion_init,
	    blood_init,
	    shards_init,
	    beam_init,
	    mflash_init,
	    drip_init,
	    spawn_init,
	    teleport_init
	};
    
    if( p.effect < NUM_EFFECTS ) {
	if( p.length == 0 ) {
	    printf("LENGTH 0 for effect %d?\n", p.effect);
	    return;
	}
	if( p.color1 < NUM_COLORS && p.color2 < NUM_COLORS )
	    effect[p.effect](p.dir, p.x, p.y, p.length, p.color1, p.color2);
	else
	    printf("Invalid colors (%d,%d) for effect %d!\n",
		   p.color1, p.color2, p.effect);
    } else
	printf("Unknown particle effect %d\n", p.effect);

}


extern char *colortab[NUM_COLORS];

/* Removes all particles for all colours */
void particle_clear(void)
{
    particles_t *ptl, *next;
    int col;

    for( col=0 ; col < NUM_COLORS ; col++ ) {
	/* Traverse list & delete all nodes */
	for( ptl = ptl_tab[col].root ; ptl != NULL ; ptl = next ) {
	    next = ptl->next;
	    particles_delete(col, ptl);
	}
    }

}


void particle_update(float secs)
{
    particles_t *ptl, *next;
    msec_t now;
    int col;
    int i;

    now = gettime();
    for( col=0 ; col < NUM_COLORS ; col++ ) {
	ptl = ptl_tab[col].root;

	while( ptl != NULL ) {
	    next = ptl->next;
	    if( now >= ptl->expire )
		particles_delete(col, ptl);
	    else if( ptl->stationary == 0 ) {
		/* Update particle positions */
		for( i=0 ; i < ptl->num_ptl ; i++ ) {
		    ptl->pos[i].x += ptl->vel[i].x * secs;
		    ptl->pos[i].y += ptl->vel[i].y * secs;
		}
	    }
	    ptl = next;
	}
    }

}


void particles_draw(ptl_level_t level)
{
    particles_t *ptl;
    float x_off, y_off;
    int col, i;
    int j; /* j = number of particles of a particular color to be drawn */
    
    x_off = client.screenpos.x + 0.5;
    y_off = client.screenpos.y + 0.5;
    
    for( col=0 ; col < NUM_COLORS ; col++ ) {
	j = 0;
	for( ptl = ptl_tab[col].root ; ptl != NULL ; ptl = ptl->next ) {
	    if( ptl->level == level ) {
		for( i=0 ; i < ptl->num_ptl && j < MAXPOINTS ; i++ ) {
		    /* Copy particles to the xp array */
		    xp[j].x = ptl->pos[i].x - x_off;
		    xp[j].y = ptl->pos[i].y - y_off;
		    j++;
		}
	    }
	}

	if( j )
	    draw_pixels(win.buf, xp, j, colortab[col]);

    }

}


