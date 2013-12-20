#include <stdio.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "world.h"
#include "weapon.h"
#include "event.h"
#include "sv_map.h"

extern int class_population[NUM_ENT_CLASSES];
extern map_t *map;
extern byte num_weapon_types;
extern server_t server;

/* Update the game world by msec milli-seconds */
void world_update(float secs)
{

    entity_update(secs);
    weapon_update();

}


/* Friction in pixels / second (soon to be in a config file) */
#define FRICTION 500.0
#define MIN_SPEED 5.0
void world_friction(entity *ent, float secs)
{

    /* X vel */
    if( ent->x_v > MIN_SPEED ) {
	if( FRICTION * secs > ent->x_v )
	    ent->x_v *= 0.8;
	else
	    ent->x_v -= FRICTION * secs;
    } else if( ent->x_v < -MIN_SPEED ) {
	if( -FRICTION * secs < ent->x_v )
	    ent->x_v *= 0.8;
	else
	    ent->x_v += FRICTION * secs;
    } else
	ent->x_v = 0.0;

    /* Y vel */
    if( ent->y_v > MIN_SPEED ) {
	if( FRICTION * secs > ent->y_v )
	    ent->y_v *= 0.8;
	else
	    ent->y_v -= FRICTION * secs;
    } else if( ent->y_v < -MIN_SPEED ) {
	if( -FRICTION * secs < ent->y_v )
	    ent->y_v *= 0.8;
	else
	    ent->y_v += FRICTION * secs;
    } else
	ent->y_v = 0.0;

}


void world_collision(entity *ent, float *dirvel)
{

    switch( ent->class ) {
    case GOODIE:
    case BADDIE:
	if( ent->mode == PATROL ) {
	    *dirvel = -*dirvel;
	    if( dirvel == &ent->x_v )
		ent->dir = DEGREES - ent->dir;
	    else
		ent->dir = DEGREES / 2 - ent->dir;
	} else
	    *dirvel = 0.0;
	break;
    case PROJECTILE:
	ent->health = 0;
	*dirvel = 0.0;
	break;
    default:
	*dirvel = 0.0;
    }
}


enum {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    NUM_CORNERS
};

#define OUT_OF_RANGE(a) \
  (a.x >= map->width*TILE_W || a.x < 0 || a.y >= map->height*TILE_H || a.y < 0)


/* If move is ok, then move entity */
int world_check_entity_move(entity *ent, float secs)
{
    vector_t pos; /* Position next frame */
    point mp; /* Map Position (in tile units) */
    float x, y; /* Co-ords of ground clip box */
    float *dirvel, *dirpos;
    int gcb_height, gcb_offset;
    int dirtile, stoppos, sidelength;
    int corner, i;
    int out_of_range;
    int cliplevel;
    int ok = 1;
    byte e;

    /* If the entity is stationary and the seconds are non-zero then skip.
       in checking whether the initial entities are clipping, seconds will
       be zero (when we want to test), otherwise they should always be > 0 */
    if( !(ent->x_v || ent->y_v) && secs )
	return ok;

    if( ent->mode < DYING )
	return ok;

    /* Quick case to kill projectiles moving offscreen */
    if( ent->y <= 0 && ent->class == PROJECTILE ) {
	world_collision(ent, &ent->y_v);
	return ok;
    }

    cliplevel = ent->cliplevel;
    /* Ground Clip Box is used because entities are on a perspective */
    gcb_height = ent->type_info->height/5 * 2;
    gcb_offset = ent->type_info->height - gcb_height;
    /* GCB co-ordinates */
    x = ent->x;

    if( ent->y < 0 )
	y = 0;
    else
	y = ent->y + gcb_offset;

    /* Position NEXT frame */
    pos.x = x;
    pos.y = y;

    for( i = 0 ; i < NUM_DIMENSIONS ; i++ ) {
	if( i == X ) {
	    dirvel = &ent->x_v;
	    pos.x += *dirvel * secs;
	    dirpos = &pos.x;
	    sidelength = ent->type_info->width;
	} else {
	    dirvel = &ent->y_v;
	    pos.y += *dirvel * secs;
	    dirpos = &pos.y;
	    sidelength = gcb_height;
	}

	for( corner = 0 ; corner < NUM_CORNERS ; corner++ ) {
	    mp.x = pos.x;
	    mp.y = pos.y;
	    if( corner == TOP_RIGHT || corner == BOTTOM_RIGHT ) {
		mp.x += ent->type_info->width;
	    }
	    if( corner == BOTTOM_LEFT || corner == BOTTOM_RIGHT ) {
		mp.y += gcb_height;
	    }
	    /* Check if entity is out of range (in pixel co-ordinates) */
	    out_of_range = OUT_OF_RANGE(mp);
	    /* Convert mp to map units */
	    mp.x /= TILE_W;
	    mp.y /= TILE_H;

	    /* Two corners CLOSEST to the directions origin */
	    if( corner == TOP_LEFT ||
		(i == X && corner == BOTTOM_LEFT) ||
		(i == Y && corner == TOP_RIGHT) ) {
		dirtile = i? y/TILE_W : x/TILE_W;
		stoppos = dirtile * TILE_H;
	    } else {
		dirtile = i? mp.y : mp.x;
		stoppos = dirtile * TILE_W - (sidelength + 1);
	    }

	    /* Hit something */
	    if( out_of_range || map->base[mp.y*map->width+mp.x] >= cliplevel ){
		ok = 0;
		*dirpos = stoppos;
		world_collision(ent, dirvel);
		if( ent->health <= 0 )
		    return ok;
	    }

	    if( !out_of_range && map->event) { /* Check event layer */
		if( (e = map->event[mp.y*map->width+mp.x]) ) {
		    switch( event_trigger(ent, e) ) {
		    case 0: /* Movement  not changed... */
			break;
		    case 1: /* Collision... */
			ok = 0;
			*dirpos = stoppos;
			world_collision(ent, dirvel);
			if( ent->health <= 0 )
			    return ok;
			break;
		    default: /* Something else..... return */
			return ok;
		    }
		}
	    }
	}
    }

    /* Set ent to updated position */
    ent->x = pos.x;
    ent->y = pos.y - gcb_offset;
    if( ent->y < 0 )
	ent->y = 0;

    return ok;

}
