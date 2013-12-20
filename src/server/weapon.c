#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "sv_net.h"
#include "weapon.h"
#include "sv_map.h"
#include "hitscan.h"
#include "ai.h"

extern map_t *map;
extern entity *root;
extern entity *tail;
extern server_t server;
extern byte num_entity_types;

byte num_weapon_types;

static void muzzle_flash(point P, byte dir);

void weapon_init(void)
{
    ent_type_t *et;
    int weapon;
    int i;

    num_weapon_types = weapon_type_init();

    /* Match weapon names to weapon types */
    for( i=0 ; i < num_entity_types ; i++) {
	if( (et = entity_type(i)) == NULL || et->weapon_name[0] == '\0' )
	    continue;

	if( et->class == ITEM && et->item_action == GIVEWEAPON ) {
	    /* The Weapon a GIVEWEAPON item gives you */
	    if( (weapon = match_weapon_type( et->weapon_name )) >= 0 )
		et->item_type = weapon;
	    else {
		printf("Couldn't match weapon type for item %s\n", et->name);
		et->item_type = 0;
	    }
	} else if( et->weapon_name ) {
	    /* Default weapons for entities */
	    if( (weapon = match_weapon_type( et->weapon_name )) >= 0 )
		et->weapon = weapon;
	    else {
		printf("Couldn't match weapon type for entity %s\n", et->name);
		et->weapon = 0;
	    }
	}

    }

}


void weapon_update(void)
{
    entity *ent;
    int weap, i;

    for( ent = root ; ent != NULL ; ent = ent->next ) {
	if( ent->trigger )
	    weapon_fire(ent);

	if( ent->weapon_switch  && !( ent->powerup & ( 1<<PU_FIXED_WEAPON ) ) ) {
	    if( server.now - ent->last_switch >= WEAPON_SWITCH_TIME ) {

		weap = ent->weapon;
		i = 0; /* Amount of weapons we have tried */
		do {
		    if( i++ >= num_weapon_types ) {
			printf("entity has no weapons!\n");
			weap = WT_NONE;
			break;
		    }
		    
		    /* Increment within range */
		    weap += ent->weapon_switch;
		    if( weap >= num_weapon_types )
			weap = 1;
		    else if( weap < 1 )
			weap = num_weapon_types - 1;

		} while( ent->has_weapon[ weap ] == 0 );

		ent->weapon = weap;
		ent->last_switch = server.now;
		ent->weapon_switch = 0;

	    }
	}

    }

}


extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];

void weapon_fire(entity *ent)
{
    weap_type_t *wt;
    ent_type_t *et, *pt;
    entity *proj;
    msec_t reload_time;
    int i;
    ammunition_t *ammo, ammo_needed;
    float x, y;
    float x_v, y_v;
    point start;
    byte dir, d;

    if( (wt = weapon_type(ent->weapon)) == NULL ) {
	printf("Error firing weapon %d for entity %d (%s)\n", 
	       ent->weapon, ent->id, ent->type_info->name);
	return;
    }

    reload_time = wt->reload_time;
    if( ent->powerup & (1<<PU_DOUBLE_FIRE_RATE) )
	reload_time /= 2;

    if( server.now - ent->last_fired < reload_time ) {
	/* Send client "click" sound? */
	return;
    }

    et = ent->type_info;

    if( wt->ammo_type != AMMO_INFINITE ) {
	ammo = &ent->ammo[ wt->ammo_type ];

	if( *ammo < 1 ) {
	    *ammo = 0;
	    ent->weapon = et->weapon;
	    ent->last_switch = server.now;
	    return;
	}

	ammo_needed = wt->ammo_usage;
	if( wt->class == WT_BEAM )
	    ammo_needed /= server.fps + 0.5; /* Round */

	if( ent->powerup & (1<<PU_HALF_AMMO_USAGE) ) {
	    if( (rand()%2) == 0 ) /* Avg = 50% usage */
		ammo_needed = 0;
	}

	if( *ammo < ammo_needed )
	    return;
	else
	    *ammo -= ammo_needed;
    }

    ent->last_fired = server.now;
    start.x = ent->x + et->width/2;
    start.y = ent->y + et->height - (et->height/5 * 2);
    /* adjust y to top of ground clip box (see world.c) */

    d = ent->dir + 16;
    d /= 32;
    d *= 32;

    /* Get it outside shooters body */
    start.x += sin_lookup[d] * (et->width/2);
    start.y += -cos_lookup[d] * (et->height/2);

    if( wt->muzzle_flash )
	muzzle_flash(start, d);

    for( i=0 ; i < wt->number ; i++ ) {
	dir = ent->dir;

	if( wt->spread )
	    dir += rand()%(wt->spread * 2) - wt->spread;

	switch( wt->class ) {
	case WT_NONE:
	    break;
	case WT_PROJECTILE:
	    /* Only 1/2 relative motion, but it makes it easier to aim */
	    x_v = ent->x_v / 2;
	    y_v = ent->y_v / 2;
	    x_v += sin_lookup[ent->dir] * wt->speed;
	    y_v -= cos_lookup[ent->dir] * wt->speed;

	    if( (pt = entity_type(wt->projectile)) == NULL ) {
		wt->projectile = 0;
		return;
	    }
	    x = start.x - pt->width/2;
	    y = start.y - pt->height + pt->height/5 * 2;
	    proj = entity_new( wt->projectile, x, y, x_v, y_v);
	    proj->dir = ent->dir;
	    proj->pid = ent->id; /* Set parent id to the projectiles shooter */
	    /* If the projectile hits something, the projectile's weapon value
	       (which is the weapon that caused the projectile to be fired)
	       will be the one used to call weapon_hit() */
	    proj->weapon = ent->weapon;
	    break;
	default:
	    hitscan_fire( ent, ent->weapon, start, dir );
	}
    }

}


/* Shooter hit victim at point P with weapon of type weapon */
void weapon_hit(entity *shooter, entity *victim, point P, byte weaptype)
{
    ent_type_t *et;
    weap_type_t *wt;
    int damage;
    netmsg msg;
    byte dir;

    wt = weapon_type(weaptype);

    if( victim == NULL ) {
	printf("VICTIM = NULL!\n");
	return;
    } else
	et = victim->type_info;

    /* Work out if we should be here first */
    if( victim->class == ITEM || victim->health <= 0 )
	return;
    
    damage = wt->damage;
    if( wt->class == WT_BEAM ) { /* Damage is done over a second for beams */ 
	damage /= server.fps;
	if( damage < 1 ) {
	    printf("Fuxored damage\n");
	    damage = 1;
	}
    }

    /* Resistance reduces damage by 50% */
    if( victim->powerup & (1<<PU_RESISTANCE) )
	damage /= 2;

    victim->health -= damage;

    if( wt->push_factor ) {
	dir = calculate_dir(shooter, victim);
	victim->x_v += sin_lookup[ dir ] * damage * wt->push_factor;
	victim->y_v -= cos_lookup[ dir ] * damage * wt->push_factor;
    }

    if( victim->health <= 0 )
	entity_killed(shooter, victim, P, weaptype);
    else if( victim->ai == AI_FIGHT ) {
	victim->target = shooter; /* Target the entity that just shot them */
    }

    /* Spawn blood particles if victim bleeds */
    if( et->bleeder ) {
	msg.type = NETMSG_PARTICLES;
	msg.particles.effect = P_BLOOD;
	msg.particles.dir = shooter? shooter->dir : 0;
	msg.particles.length = damage;
	msg.particles.color1 = COL_RED;
	msg.particles.color2 = COL_RED;
	msg.particles.x = P.x;
	msg.particles.y = P.y;
	sv_net_send_to_all(msg);
    }

}


void weapon_explode(entity *explosion, int radius, int damage )
{
    netmsg msg;
    entity *victim, *shooter;
    point P;
    int d1, dist, hurt;
    int i;
    byte dir;

    /* Where the explosion starts */
    P.x = explosion->x + explosion->type_info->width/2;
    P.y = explosion->y + explosion->type_info->height/2;

    /* Draw explosion */
    msg.type = NETMSG_PARTICLES;
    msg.particles.effect = P_EXPLOSION;
    msg.particles.dir = 0;
    msg.particles.length = (damage + radius) * 20;
    msg.particles.color1 = COL_ORANGE;
    msg.particles.color2 = COL_RED;
    msg.particles.x = P.x;
    msg.particles.y = P.y;
    sv_net_send_to_all(msg);

    /* Make some soot at the site of explosions */


    msg.particles.effect = P_SHARDS;
    msg.particles.color1 = COL_BLACK;
    msg.particles.color2 = COL_GRAY;

    for( i=0 ; i<10 ; i++ ) {
	dir = rand();
	msg.particles.x = P.x + sin_lookup[ dir ] * i * 5;
	msg.particles.y = P.y - cos_lookup[ dir ] * i * 5;
	sv_net_send_to_all(msg);
    }

    /* Find the person who shot the exploding weapon */
    if( (shooter = findent(explosion->pid)) == NULL )
	return; /* Shooter is no longer in the game */

    /* Do explosion damage */
    for( victim = root ; victim != NULL ; victim = victim->next ) {
	if( victim->class == ITEM || victim->health <= 0 )
	    continue;

	dist = entity_dist(explosion, victim);

	/* Account for entity's width in factoring distance from explosion */
	d1 = MIN( victim->type_info->width, victim->type_info->height ) / 2;
	if( dist > d1 )
	    dist -= d1;

	/* Apply damage if entity is within blast range */
	if( dist < radius ) {
	    hurt = damage * (radius - dist) / radius;
	    if( victim->powerup & (1<<PU_RESISTANCE) )
		hurt /= 2;

	    victim->health -= hurt;
	    if( victim->health <= 0 )
		entity_killed(shooter, victim, P, explosion->weapon);
	}
    }

}


void weapon_reset(entity *ent)
{
    weap_type_t *wt;

    wt = weapon_type(ent->type_info->weapon);
    memset(ent->has_weapon, 0, num_weapon_types);
    ent->has_weapon[ent->type_info->weapon] = 1; /* Has default weapon */
    ent->ammo[wt->ammo_type] = wt->initial_ammo;
    ent->weapon = ent->type_info->weapon;

}

static void muzzle_flash(point P, byte dir)
{
    netmsg msg;

    msg.type = NETMSG_PARTICLES;
    msg.particles.effect = P_MFLASH;
    msg.particles.dir = dir;
    msg.particles.length = 50;
    msg.particles.color1 = COL_YELLOW;
    msg.particles.color2 = COL_RED;
    msg.particles.x = P.x;
    msg.particles.y = P.y;
    sv_net_send_to_all(msg);

}
