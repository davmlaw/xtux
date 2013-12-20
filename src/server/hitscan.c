/* Hitscan.c: Weapons that have instant hit capabilities
   Author: David Lawrence
   Started: May 07 2000
*/

#include <stdio.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "weapon.h"
#include "clients.h"
#include "sv_net.h"
#include "hitscan.h"


extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];
extern entity *root;
extern map_t *map;


void hitscan_fire( entity *ent, byte weapon, point weap_start, byte dir )
{
    weap_type_t *wt;
    point weap_end;
    int hitsomething = 0;
    netmsg msg;
    netushort length;

    wt = weapon_type(weapon);

    if( wt->wallstop ) {
	/* length = How far the weapon travelled till it hit a wall */
	length = hitscan_wall(weap_start, dir);
	if( wt->max_length ) {
	    if( length > wt->max_length )
		length = wt->max_length;
	}
    } else
	length = wt->max_length;

    weap_end = weap_start;
    weap_end.x += sin_lookup[dir] * length;
    weap_end.y -= cos_lookup[dir] * length;

    if( hitscan_entity(weap_start, weap_end, ent, &length, wt->entstop) ) {
	hitsomething = 1;
	weap_end = weap_start;
	weap_end.x += sin_lookup[dir] * length;
	weap_end.y -= cos_lookup[dir] * length;
    }

    /**** All we have to do now is send client particle effects *****/

    if( length <= 0 || wt->draw_particles == 0 )
	return; /* No particles to draw */

    /* Common values for each netmsg we could send */
    msg.type = NETMSG_PARTICLES;
    msg.particles.dir = dir;

    /* Draw the weapon effects */
    switch( wt->class ) {
    case WT_SLUG:
	msg.particles.effect = P_RAILSLUG;
	msg.particles.color1 = ent->color1;
	msg.particles.color2 = ent->color2;
	msg.particles.x = weap_start.x;
	msg.particles.y = weap_start.y;
	msg.particles.length = length;
	sv_net_send_to_all(msg);
	break;
    case WT_BEAM:
	msg.particles.effect = P_BEAM;
	msg.particles.color1 = ent->color1;
	msg.particles.color2 = ent->color2;
	msg.particles.x = weap_start.x;
	msg.particles.y = weap_start.y;
	msg.particles.length = length;
	sv_net_send_to_all(msg);
	break;
    case WT_BULLET:
	if( hitsomething == 0 ) {
	    /* Draw those little shards if it hit a wall */
	    msg.particles.effect = P_SHARDS;
	    msg.particles.color1 = COL_YELLOW;
	    msg.particles.color2 = COL_GRAY;
	    msg.particles.x = weap_end.x;
	    msg.particles.y = weap_end.y;
	    msg.particles.length = length;
	    sv_net_send_to_all(msg);
	}
	break;
    default:
	break;
    }



}





/*
  Checks to see if a weapon fired from A to B hits anything.
  If "stops" is 1, then the weapons bullet/projectile stops after hitting
  the CLOSEST TARGET to the weapons start (A). This is not necessarily the
  first entity that we get a successful intersection test with.

  If stops = 0, the weapon affects every entity it passes through.

  Returns true if it hits something.
*/
int hitscan_entity(point A, point B, entity *shooter,
			netushort *length, int stops)
{
    entity *ent, *next, *victim;
    point TL, TR, BL, BR;    /* (Top/Bottom) / (Left/Right) */
    point P; /* Point of interesection */
    int h = 0; /* hit anything boolean */

    victim = NULL; /* The entity that gets hit */
    for( ent = root ;  ent != NULL ; ent = next ) {
	next = ent->next;
	
	if( ent == shooter )
	    continue;	/* disregard firer of weapon */

	TL.x = ent->x;
	TL.y = ent->y;

	TR.x = ent->x + ent->type_info->width;
	TR.y = ent->y;

	BL.x = ent->x;
	BL.y = ent->y + ent->type_info->height;

	BR = BL;
	BR.x += ent->type_info->width;

	/* WRONG!!! NEEDS TO BE CALC'D FOR EACH DIRECTION */

	/* Trivial rejection? */


	h = 0;
	h += intersection_test(A, B, TL, BL, &P);
	h += intersection_test(A, B, TR, BR, &P);
	h += intersection_test(A, B, TL, TR, &P);
	h += intersection_test(A, B, BL, BR, &P);

	/*
	if( A.x <= TR.x ) // Test Left side
	    h += intersection_test(A, B, TL, BL, &P);

	if( A.x >= TL.x ) // Test Right side
	    h += intersection_test(A, B, TR, BR, &P);

	if( A.y <= BL.y ) // Test Top side
	    h += intersection_test(A, B, TL, TR, &P);

	if( A.y >= TL.y ) // Test Bottom side
	    h += intersection_test(A, B, BL, BR, &P);

	*/

	if( h ) {
	    if( stops ) {
		B = P; /* New end point of collision we check against */
		victim = ent; /* Current victim */
	    } else {
		/* Everything in the path gets hit */
		weapon_hit(shooter, ent, P, shooter->weapon);
	    }
	}
    }

    /* Inflict the damage on the final victim (if it exists) */
    if( stops && victim != NULL )
	weapon_hit(shooter, victim, B, shooter->weapon);	

    *length = point_distance( A, B );
    return h;

}


/* FIXME: Optimise! */

/* Returns the length of the hitscan weapons path */
netushort hitscan_wall(point start, byte dir)
{
    int ox, oy;
    int x, y;
    int wallpos;
    vector_t end;
    netushort length = 0;
    
    end.x = start.x;
    end.y = start.y;
    ox = -1;
    oy = -1;

    while( 1 ) {
	end.x += sin_lookup[dir];
	end.y -= cos_lookup[dir];
	x = end.x / 64;
	y = end.y / 64;
	/* Only check when map tiles change */
	if( ox != x || oy != y ) {
	    ox = x;
	    oy = y;
	    /* Out of range of map */
	    if( y < 0 || y >= map->height )
		break;
	    if( x < 0 || x >= map->width )
		break;

	    if( map->base[ y * map->width + x ] == ALLCLIP ) {
		/* Hack to get perspective looking right */
		if( y == map->height-1 )
		    break;
		if( map->base[(y+1)*map->width+x] == ALLCLIP)
		    break;
		/* Position vertically on the wall it hits */
		wallpos = end.y;
		wallpos %= TILE_W;
		if( wallpos < 36 ) /* This looks about right :) */
		    break;
		else
		    ox = -1; /* So we'll check next loop */
	    }
	}
	length++;
    }

    return length;

}

