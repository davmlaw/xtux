#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "ai.h"

extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];
extern server_t server;
extern entity *root;

void ai_move(entity *ent)
{
    int accel;

    /* Move forward */
    if( ent->ai_moving && ent->mode >= ALIVE ) {
	accel = ent->type_info->accel;
	ent->x_v += sin_lookup[ ent->dir ] * accel;
	ent->y_v += -cos_lookup[ ent->dir ] * accel;
    }

}


/* Find appropriate threat and face it, setting ent->target appropriately.
   true opposite is anything but targ_type */
int ai_face_threat(entity *ent, int distance, int targ_type, int opposite)
{
    int d, found;
    entity *targ;

    /* Check to see if existing target is within distance, if so, go for it first */
    if( ent->target ) {
	if( ent->target->visible && (d = entity_dist(ent, ent->target)) < distance ) {
	    ent->dir = calculate_dir(ent, ent->target);
	    return d;
	}
    }

    found = 0;
    for( targ = root ; targ != NULL ; targ = targ->next ) {
	if( targ == ent || targ->id == ent->pid )
	    continue; /* Don't match self or parent */
	if( ent->pid != 0 && targ->pid == ent->pid )
	    continue; /* Don't match siblings */

	if( (opposite ? (targ->class != targ_type) : (targ->class == targ_type))) {
	    if( targ->visible && (d = entity_dist(ent, targ)) < distance ) {
		found = 1;
		distance = d;
		ent->dir = calculate_dir(ent, targ);
		ent->target = targ;
	    }
	}
    }

    if( found )
	return distance;
    else
	return -1; /* No threats near */

}


#define SCARED_DIST 200

/* AI for bunny (and critters).
 * They look at threats that are near them and run away from them
 * if they get too close.
 * If nobody's around, they groom themselves and hop around.
 */
void ai_flee_think(entity *ai)
{
    int speed, sight;

    sight = ai->type_info->sight;
    speed = ai->type_info->speed;

    if( ai_face_threat(ai, sight, NEUTRAL, 1) >= 0 ) {
	ai->dir += DEGREES/2; /* Face AWAY from threat. */
	ai->ai_moving = 1;
	ai->mode = ALIVE; /* Stop Fidgeting */
	ai->speed = speed;
    } else if( ai->mode == ALIVE && (ai->x_v || ai->y_v) ) {
	if( (rand()%(server.fps*5)) == 0 ) {
	    ai->mode = FIDGETING;
	    ai->ai_moving = 0;
	}
    } else if( ai->mode == ALIVE ) {
	ai->speed = speed/2;
	ai->ai_moving = 1;
    }

    if( ai->mode == FIDGETING ) {
	/* Move around a bit */
	if( ((rand() % (10*server.fps))) == 0 ) {
	    ai->mode = ALIVE;
	    ai->dir = rand()%256;
	    ai->ai_moving = 0;
	}
    }

}


/* FIXME: Todo */
void ai_fight_think(entity *ai)
{
    int target;
    
    ai->speed = ai->type_info->speed;

    if( ai->class == GOODIE )
	target = BADDIE;
    else 
	target = GOODIE;


    if( ai_face_threat(ai, ai->type_info->sight, target, 0) >= 0 ) {
	ai->ai_moving = 1;
	ai->mode = ALIVE;
	ai->cliplevel = GROUNDCLIP;
    } else if( ai->mode == PATROL ) {
	ai->cliplevel = AICLIP;
	ai->ai_moving = 1;
    } else if( (ai->x_v || ai->y_v) == 0 ) { /* Stationary */
        if( ai->mode != PATROL || !(rand() % (10*server.fps)) ) {
	    ai->mode = PATROL;
	    ai->dir = rand()%DEGREES;
	    ai->ai_moving = 1;
	}
    }

}


void ai_shooter_think(entity *shooter)
{
    weap_type_t *wt;
    int dist, target, effective_range;
    int sight;

    shooter->ai_moving = 1;
    shooter->trigger = 0;
    shooter->speed = shooter->type_info->speed;
    sight = shooter->type_info->sight;

    if( shooter->class == GOODIE )
	target = BADDIE;
    else
	target = GOODIE;

    dist = ai_face_threat(shooter, sight, target, 0);

    if( dist >= 0 ) {
	wt = weapon_type(shooter->weapon);
	if( wt->max_length )
	    effective_range = wt->max_length;
	else
	    effective_range = sight;

	shooter->mode = ALIVE;
	if( dist < effective_range ) {
	    shooter->ai_moving = 0;
	    shooter->trigger = 1;
	} else {
	    shooter->trigger = 0;
	    shooter->ai_moving = 1;
	    shooter->cliplevel = GROUNDCLIP;
	}
    } else if( shooter->mode == PATROL ) {
	shooter->cliplevel = AICLIP;
	shooter->ai_moving = 1;
    } else if( shooter->mode != PATROL || !(rand() % (10*server.fps)) ) {
        shooter->mode = PATROL;
        shooter->dir = rand()%DEGREES;
    }

}



void ai_seek_think(entity *ai)
{

    if( ai->target ) {
	ai->dir = calculate_dir(ai, ai->target);
    } else if( ai_face_threat(ai, ai->type_info->sight, ITEM, 1) >= 0 ) {
	ai->ai_moving = 1;
    } else {
	ai->ai_moving = 0;
    }

}



/* Find the dir ent must face to see target */
byte calculate_dir(entity *ent, entity *target)
{
    float ex, ey;
    float tx, ty;
    vector_t U, V;

    /* U = unit vector in dir 0 from origin */
    U.x = 0;
    U.y = 1;

    /* (ex, ey) = midpoint of ent */
    ex = ent->x + ent->type_info->width/2;
    ey = ent->y + ent->type_info->height/2;

    /* (tx, ty) = midpoint of target */
    tx = target->x + target->type_info->width/2.0;
    ty = target->y + target->type_info->height/2.0;

    /* V = vector from (origin) entity -> target */
    V.x = tx - ex;
    V.y = -1 * (ty - ey);

    return angle_between(U,V);

}

/* This is a terrible hack to make the weapons spin.... */

/* Offset from (0, (DIR) * 50) to the center of the gun, which
   was found by using the co-ordinates in the gimp, I did say this was
   a terrible hack... */
static int gun_pos[8][2] = {
    { 41, 15 },
    { 45, 24 },
    { 38, 32 },
    { 23, 34 },
    { 12, 28 },
    { 41, 34 },
    { 27, 32 },
    { 18, 25 }
};

#define MSECS_PER_ROTATION 1500

void ai_rotate_think(entity *ai)
{
    int olddir, dir;
    int x = 0, y = 0;

    olddir = (ai->dir + 16)/32;
    ai->dir += (256 / ((float)server.fps / M_SEC)) / MSECS_PER_ROTATION;
    dir = (ai->dir + 16)/32;

    if( dir != olddir ) {
	x  = gun_pos[dir%8][0] - gun_pos[olddir%8][0];
	y  = gun_pos[dir%8][1] - gun_pos[olddir%8][1];
	ai->x -= x;
	ai->y -= y;
    }

}
