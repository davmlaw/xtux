#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "sv_map.h"
#include "weapon.h"
#include "world.h"
#include "sv_net.h"
#include "ai.h"
#include "item.h"
#include "game.h"

extern server_t server;
extern game_t game;
extern byte num_weapon_types;

/* Start and end of entity list, initially NULL */
entity *root;
entity *tail;

int num_entities;
byte num_entity_types;

/* Keeps track of how entities of each class there are */
int class_population[NUM_ENT_CLASSES];

static int id_num = 1;

static void entity_update_movement(float secs);
static void entity_check_collisions(void);
static void entity_animate(void);
static void entity_die(entity *ent);


/* Called once at program initialisation */
void entity_init(void)
{
    num_entity_types = entity_type_init();
}


void entity_drip(entity *ent, byte color1, byte color2)
{
    netmsg msg;
    
    msg.type = NETMSG_PARTICLES;
    msg.particles.effect = P_DRIP;
    msg.particles.dir = ent->dir;
    msg.particles.length = 1;
    msg.particles.color1 = color1;
    msg.particles.color2 = color2;
    /* Draw with offsets to make it look good on the client */
    msg.particles.x = ent->x + ent->type_info->width/2;
    msg.particles.y = ent->y + ent->type_info->height/2;
    sv_net_send_to_all(msg);

}


void entity_spawn_effect(entity *ent)
{
    netmsg msg;

    /* Send respawning effect to clients */
    msg.type = NETMSG_PARTICLES;
    msg.particles.effect = P_SPAWN;
    msg.particles.color1 = COL_WHITE;
    msg.particles.color2 = COL_GRAY;
    msg.particles.length = ent->type_info->width / 2;
    msg.particles.x = ent->x + ent->type_info->width/2;
    msg.particles.y = ent->y + ent->type_info->height/2;
    sv_net_send_to_all(msg);

}

/* How often the entity drips while invisible */
#define INVISIBLE_DRIP_TIME 500

void entity_update(float secs)
{
    ent_type_t *et;
    entity *ent, *next;
    msec_t now, time;
    int i;

    now = server.now;

    for( ent = root ; ent != NULL ; ent = ent->next ) {
	et = ent->type_info;



	if( ent->dripping ) {
	    time = (ent->dripping==1)? et->drip_time : INVISIBLE_DRIP_TIME;
	    if( now - ent->last_drip >= time ) {
		entity_drip(ent, ent->drip1, ent->drip2);
		ent->last_drip = now;
	    }
	}

	if( ent->lookatdir > 0 ) {
	    ent->speed = 0;
	    /* make the entity slowly turn towards the lookatpoint */
	    if( ent->dir > ent->lookatdir )
		ent->dir -= MAX( 1, (ent->dir - ent->lookatdir)/10);
	    else if( ent->dir < ent->lookatdir )
		ent->dir += MAX( 1, (ent->lookatdir - ent->dir)/10);
	    else
		ent->lookatdir = -1;
	} else if( ent->mode == ALIVE )
	    ent->speed = et->speed;
	else if( ent->mode == LIMBO ) { 	/* Respawn items */
	    if( ent->respawn_time && (now >= ent->respawn_time) ) {
		ent->mode = ALIVE;
		ent->respawn_time = 0;
		entity_spawn_effect(ent);
	    }
	}

	if( ent->powerup ) {
	    /* Entity is wounded */
	    if( ent->powerup & (1<<PU_WOUND) )
		if( now - ent->last_wound >= M_SEC ) {
		    entity_drip(ent, COL_RED, COL_RED);	/* Drip blood */
		    ent->health -= 2;
		    ent->last_wound = now;
		}

	    if( ent->powerup & (1<<PU_INVISIBILITY) ) {
		ent->visible = 0;
		if( ent->x_v || ent->y_v ) { /* Drip if moving */
		    ent->dripping = 2;
		    ent->drip1 = ent->color1;
		    ent->drip2 = ent->color2;
		} else {
		    ent->dripping = et->dripping;
		    ent->drip1 = et->drip1;
		    ent->drip2 = et->drip2;
		}
	    }

	    for( i=0 ; i<NUM_POWERUPS ; i++ ) {
		if( ent->powerup & (1<<i) && now > ent->powerup_expire[i] ) {
		    ent->powerup &= ~(1<<i);
		    ent->powerup_expire[i] = 0;

		    switch( i ) {
		    case PU_INVISIBILITY:
			ent->dripping = et->dripping;
			ent->drip1 = et->drip1;
			ent->drip2 = et->drip2;
			ent->visible = 1;
			break;
		    case PU_FROZEN:
			if( ent->mode == FROZEN )
			    ent->mode = ALIVE;
			break;
		    case PU_E:
			/* Worn off, entity is now on a come down... */
			if( ent->mode == ENLIGHTENED ) {
			    ent->mode = COMEDOWN;
			    ent->speed = et->speed * 0.50;
			    ent->powerup |= (1<<PU_E); /* Restore bit */
			    ent->powerup_expire[PU_E] = now + M_SEC * 10;
			} else if( ent->mode == COMEDOWN ) {
			    ent->mode = ALIVE;
			}			    
			break;
		    }
		}
	    }
	}
    }

    entity_update_movement(secs);
    entity_check_collisions();
    entity_animate();
 
    for( ent = root ; ent != NULL ; ent = next ) {
	next = ent->next;
	if( ent->mode >= FROZEN && ent->health <= 0 )
	    entity_die(ent); /* Set to either dead or dying */

	if( ent->mode == DEAD ) {
	    if( ent->controller == CTRL_CLIENT )
		spawn(ent, 1); /* Respawn */
	    else
		entity_delete(ent);
	}
    }

}


static void entity_update_movement(float secs)
{
    entity  *ent;

    for( ent = root ;  ent != NULL ; ent = ent->next ) {
	if( ent->mode >= ALIVE && ent->ai && ent->controller == CTRL_AI ) {
	    switch( ent->ai ) {
	    case AI_NONE:
		break;
	    case AI_FIGHT:
		if( ent->weapon ) {
		    ai_shooter_think(ent);
		} else
		    ai_fight_think(ent);
		break;
	    case AI_FLEE:
		ai_flee_think(ent);
		break;
	    case AI_SEEK:
		ai_seek_think(ent);
		break;
	    case AI_ROTATE:
		ai_rotate_think(ent);
		break;
	    default:
		break;
	    }
	    ai_move(ent);
	}

	if( ent->class != PROJECTILE )
	    world_friction(ent, secs);

	if( ent->mode >= ALIVE ) {
	    if( ent->x_v > 0 ) {
		if( ent->x_v > ent->speed )
		    ent->x_v = ent->speed;
	    } else if( ent->x_v < 0 ) {
		if( ent->x_v < -ent->speed )
		    ent->x_v = -ent->speed;
	    }
	
	    if( ent->y_v > 0 ) {
		if( ent->y_v > ent->speed )
		    ent->y_v = ent->speed;
	    } else if( ent->y_v < 0 ) {
		if( ent->y_v < -ent->speed )
		    ent->y_v = -ent->speed;
	    }
	}

	world_check_entity_move(ent, secs);

    }

}


/*

    Here's the order (from ../common/entity_type.h). You only need
    to handle collision between first and the second->class's that
    come after first's->class.

    GOODIE,
    BADDIE,
    NEUTRAL,
    ITEM,
    PROJECTILE,
    NUM_ENT_CLASSES
*/


static void col_goodie(entity *goodie, entity *second)
{
    entity *shooter;
    point pt;
    int damage;

    switch( second->class ) {
    case GOODIE:
	/* Nothing.... */
	break;
    case BADDIE:
	/* Touch damage is delt over a second */
	if( (damage = second->type_info->touchdamage / server.fps ) < 1 )
	    damage = 1; /* Correct for rounding down */
	goodie->health -= damage;
	if( goodie->health <= 0 )
	    entity_killed(second, goodie, pt, 0);
	/* Slow the baddie down while he does damage */
	second->x_v *= 0.25;
	second->y_v *= 0.25;
	break;
    case NEUTRAL:
	/* Nothing... */
	break;
    case ITEM:
	item_collision(second, goodie);
	break;
    case PROJECTILE:
	shooter = findent(second->pid);
	pt.x = second->x;
	pt.y = second->y;
	weapon_hit( shooter, goodie, pt, second->weapon );
	second->health = 0; /* Remove projectile */
	break;
    default:
	printf("NOT HANDLED!\n");
    }

}


static void col_baddie(entity *baddie, entity *second)
{
    entity *shooter;
    point pt;
    int damage;

    switch( second->class ) {
    case BADDIE:
	/* Do damage if baddies are fighting */
	if( baddie->target == second ) {
	    if( (damage = baddie->type_info->touchdamage / server.fps) < 1 )
		damage = 1; /* Correct for rounding down */
	    second->health -= damage;
	    second->target = baddie;
	} else if( second->target == baddie ) {
	    if( (damage = second->type_info->touchdamage / server.fps) < 1 )
		damage = 1; /* Correct for rounding down */
	    baddie->health -= damage;
	    baddie->target = second;
	}
	break;
    case NEUTRAL:
	/* Nothing.... */
	break;
    case ITEM:
	/* item_collision(second, baddie); */
	break;
    case PROJECTILE:
	shooter = findent(second->pid);
	pt.x = second->x;
	pt.y = second->y;
	weapon_hit( shooter, baddie, pt, second->weapon);
	second->health = 0; /* Remove projectile */
	break;
    default:
	printf("NOT HANDLED!\n");
    }

}


static void col_neutral(entity *neutral, entity *second)
{
    entity *shooter;
    point pt;

    switch( second->class ) {
    case NEUTRAL:
	break;
    case ITEM:
	/* Nothing.... */
	break;
    case PROJECTILE:
	shooter = findent(second->pid);
	pt.x = second->x;
	pt.y = second->y;
	weapon_hit( shooter, neutral, pt, second->weapon);
	second->health = 0; /* Remove projectile */
	break;
    default:
	printf("NOT HANDLED!\n");
    }

}


static void col_item(entity *item, entity *second)
{

    switch( second->class ) {
    case ITEM:
	/* Nothing.... */
	break;
    case PROJECTILE:
	/* Nothing.... */
	break;
    default:
	printf("NOT HANDLED!\n");
    }

}


/* In theory this should only be called with both first and second
   being projectiles */
static void col_projectile(entity *proj, entity *second)
{
    entity *shooter;
    point pt;

    switch( second->class ) {
    case PROJECTILE:
	/* Projectile hits second */
	if( (shooter = findent(proj->pid)) != NULL ) {
	    pt.x = proj->x;
	    pt.y = proj->y;
	    weapon_hit( shooter, second, pt, proj->weapon);
	}

	/* Second hits projectile */
	if( (shooter = findent(second->pid)) != NULL ) {
	    pt.x = second->x;
	    pt.y = second->y;
	    weapon_hit( shooter, proj, pt, second->weapon);
	}

	break;
    default:
	printf("NOT HANDLED!\n");
    }

}


static void (*handle_collision[NUM_ENT_CLASSES])(entity *,  entity *) = {
    col_goodie,
    col_baddie,
    col_neutral,
    col_item,
    col_projectile
};


/* FIXME: Optimise and make correct (use intersection tests). */
void entity_check_collisions(void)
{
    entity *ent1, *ent2;
    entity *first, *second;
    int d1, d2; /* Diameters for entities 1, 2 */
    int max_dist; /* Maximum distance 1 & 2 can be apart and still clip */

    for( ent1 = root ; ent1 != NULL ; ent1 = ent1->next ) {
	if( ent1->mode < FROZEN || ent1->health <= 0 )
	    continue; /* Skip LIMBO, DEAD & DYING entities */
	d1 = MAX( ent1->type_info->width, ent1->type_info->height );
	for( ent2 = ent1->next ; ent2 != NULL ; ent2 = ent2->next ) {
	    if( ent1->class == NEUTRAL && ent2->class == NEUTRAL )
		continue;
	    if( ent2->mode < FROZEN || ent1->health <= 0 )
		continue; /* Skip LIMBO, DEAD & DYING entities */
	    if( ent1->pid == ent2->id || ent2->pid == ent1->id )
		continue; /* Don't clip on parent */
	    d2 = MAX( ent2->type_info->width, ent2->type_info->height );
	    max_dist = (d1 + d2) / 2;
	    if( entity_dist(ent1, ent2) < max_dist ) {
		/* Call the function for the "lowest" class (in the
		   entity-class enumeration) with args (lowest, highest) */
		if( ent1->class < ent2->class ) {
		    first = ent1;
		    second = ent2;
		} else {
		    first = ent2;
		    second = ent1;
		}
		handle_collision[first->class]( first, second );
	    }
	}
    }

}


/* Creates a new entity on the end of the list of type "type" and returns
   a pointer to the newly created entity */
entity *entity_new(byte type, float x, float y, float x_v, float y_v)
{
    ent_type_t *et;
    entity *ent;

    if( (et = entity_type(type)) == NULL ) {
	printf("Error, no entity of type %d\n", (int)type);
	return NULL;
    }

    /* Create entity */
    if( (ent = entity_alloc()) == NULL)
	ERR_QUIT("Error creating new entity", EXIT_FAILURE);

    memset( ent, 0, sizeof(entity) );

    /* Add it to the list */
    if( root == NULL )
	root = ent; /* The new root entity */

    if( tail != NULL )
	tail->next = ent;

    ent->prev = tail;
    tail = ent;
    tail->next = NULL;

    num_entities++;
    id_num++;

    ent->id = id_num;
    ent->x = x;
    ent->y = y;
    ent->x_v = x_v;
    ent->y_v = y_v;

    /* Set values from default type for this particular entity */
    ent->type_info = et;
    ent->type = type;
    ent->class = et->class;
    ent->mode = ALIVE;
    ent->speed = et->speed;
    ent->health = et->health;
    ent->dripping = et->dripping;
    ent->drip1 = et->drip1;
    ent->drip2 = et->drip2;
    ent->cliplevel = et->cliplevel;
    ent->color1 = COL_WHITE;
    ent->color2 = COL_BLUE;
    ent->last_fired = server.now;
    ent->controller = CTRL_AI;
    ent->visible = 1;
    ent->lookatdir = -1;

    if( num_weapon_types > 0 ) {
	if( (ent->has_weapon = (byte *)malloc(num_weapon_types)) == NULL ) {
	    perror("Malloc");
	    ERR_QUIT("Error malloc'ing has_weapon array", -1);
	}
	weapon_reset(ent);
    } else {
	ERR_QUIT("num_weapon_types out of range!\n", num_weapon_types);
    }

    memset(ent->powerup_expire, 0, sizeof(ent->powerup_expire));

    if( et->ai ) {
	ent->cliplevel = AICLIP;
	ent->ai = et->ai;
    } else { /* Default AI's for each class */
	switch( et->class ) {
	case BADDIE:
	    ent->cliplevel = AICLIP;
	case GOODIE:
	    ent->ai = AI_FIGHT;
	    break;
	case NEUTRAL:
	    ent->ai = AI_FLEE;
	    break;
	case PROJECTILE:
	    ent->ai = AI_NONE;
	    break;
	case ITEM:
	    if( et->item_action == GIVEWEAPON )
		ent->ai = AI_ROTATE;
	    break;
	}
    }

    class_population[ ent->class ]++;

    /*
      printf("created new %s (type %d, class %d) %d entities\n",
      et->name, type, ent->class, num_entities);
    */

    return ent;

}


void entity_delete(entity *ent)
{
    entity *seeker;
    weap_type_t *wt;

    /* If something has current entity as target, remove it */
    for(  seeker =  root ; seeker != NULL ; seeker = seeker->next ) {
	if( seeker->target == ent )
	    seeker->target = NULL;
    }

    /* Does entity explode when it dies? */
    if( ent->class == PROJECTILE ) {
	wt = weapon_type(ent->weapon);
	if( wt->explosion )
	    weapon_explode(ent, wt->explosion, wt->splashdamage);
    }

    if( num_weapon_types > 0 ) {
	free( ent->has_weapon );
    }

    num_entities--;
    class_population[ ent->class ]--;

    /* Make sure that root & Tail will still point at the
       right spot after the entity is deleted */
    if( root == ent )
	root = ent->next;
    if( tail == ent )
	tail = ent->prev;

    if( ent->prev )
	ent->prev->next = ent->next;

    if( ent->next )
	ent->next->prev = ent->prev;

    free(ent);

}


/* Deletes all entities, returns the number of entities deleted */
int entity_remove_all_non_clients(void)
{
    entity *ent, *next;
    int num = 0;

    /* printf("Removing all non client entities\n"); */

    ent = root;
    while( ent != NULL ) {
	num++;
	next = ent->next;
	if( ent->cl ) /* Don't delete clients... */
	    ent->mode = LIMBO; /* Limbo until they send the "ready" message */
	else
	    entity_delete(ent);
	ent = next;
    }

    return num;

}


netmsg entity_to_netmsg(entity *ent)
{
    netmsg msg;

    msg.entity.entity_type = ent->type;
    msg.entity.dir = ent->dir;
    msg.entity.mode = ent->mode;
    msg.entity.x = ent->x + 0.5; /* Round floats to integers */
    msg.entity.y = ent->y + 0.5;
    msg.entity.img = ent->img;
    msg.entity.weapon = ent->weapon;

    return msg;

}


void entity_killed(entity *shooter, entity *victim, point P, byte weaptype)
{
    char buf[TEXTMESSAGE_STRLEN], *killerboy;
    int change = 0;
    weap_type_t *wt;

    victim->health = 0; /* Just to make sure */
    victim->frame = 0;

    /* No frags for killing items or projectiles */
    if( victim->class == PROJECTILE || victim->class == ITEM ) {
	return;
    }

    if( victim == shooter ) {    /* Suicide */
	victim->frags--;
	if( victim->name ) {
	    snprintf(buf, TEXTMESSAGE_STRLEN, "%s couldn't cope",victim->name);
	    sv_net_send_text_to_all(buf);
	}
	return;
    } else {
	/* Who was the victim? */
	switch( game.mode ) {
	case SAVETHEWORLD:
	    if( victim->class != GOODIE )
		change++;
	    break;
	case HOLYWAR:
	    if( victim->class == NEUTRAL )
		change--;
	    else {
		change++;
		/* OBITUARY IF CLIENT?? */
	    }
	    break;
	default:
	    ERR_QUIT("Game mode out of range!", game.mode );
	}
    }

    if( shooter )	/* Update frag count */
	shooter->frags += change;

    if( victim->name ) {
	if( shooter ) {
	    if( shooter->name )
		killerboy = shooter->name;
	    else
		killerboy = shooter->type_info->name;

	    if( (wt = weapon_type(weaptype)) && wt->obituary )
		snprintf(buf, TEXTMESSAGE_STRLEN, wt->obituary,
			 victim->name, killerboy);
	    else
		snprintf(buf, TEXTMESSAGE_STRLEN,
			 "%s got de-mapped by %s%s", victim->name,
			 shooter->name? "" : "a ", killerboy);
	    sv_net_send_text_to_all(buf);
	}
    }

}


void entity_animate(void)
{
    animation_t *ani;
    ent_type_t *et;
    entity *ent;
    msec_t framelen;
    int animate, frames;

    for( ent = root ;  ent != NULL ; ent = ent->next ) {
	ani = entity_type_animation(ent->type_info, ent->mode);
	framelen = ani->framelen;

	animate = 0; /* Don't animate by default */
	if( ani->images > 1 ) {
	    if( ani->stationary_ani ) /* Animates when stationary */
		animate = 1;
	    else if( (ent->x_v || ent->y_v) || ent->mode == ENLIGHTENED ) {
		animate = 1;
		framelen /= 2;
	    }
	}

	if( animate ) {
	    if( framelen )
		frames = (server.now - ent->last_frame) / framelen;
	    else
		frames = 0;
	    if( frames > 0 ) {
		ent->last_frame = server.now;
		ent->frame += frames;
		if( ent->frame >= ani->frames ) {
		    /* End of this loop of animation. If the entity is
		       in it's dying animation, make it dead. */
		    if( ent->mode == DYING )
			ent->mode = DEAD;
		    else
			ent->frame = 0;
		}
	    }

	    ent->img = ani->order[ent->frame];

	} else {

	    if( ent->mode == DYING ) {
		if( (et = entity_type(ent->type)) != NULL )
		    printf("Killed off %s\n", et->name);
		ent->mode = DEAD;
	    } else
		ent->img = 0;
	}
    }

}



/* returns pixels between the CENTER of ent0 & ent1 */
float entity_dist(entity *ent0, entity *ent1)
{
    float x_dist, y_dist;

    x_dist = (ent0->x + ent0->type_info->width/2)
	     - (ent1->x + ent1->type_info->width/2);
    y_dist = (ent0->y + ent0->type_info->height/2)
	     - (ent1->y + ent1->type_info->height/2);
    return sqrt( x_dist * x_dist + y_dist * y_dist );

}


/* Find entity that has the entity-id of id */ 
entity *findent(int id)
{
    entity *ent;

    for( ent=root ; ent != NULL ; ent = ent->next ) {
	if( ent->id == id )
	    return ent;
    }

    return NULL;

}


/* Called when victim's health <= 0 */
static void entity_die(entity *ent)
{
    
    if( ent->type_info->animation[ DYING ] ) {
	ent->mode = DYING;
	ent->last_frame = server.now;
	ent->frame = 0;
    } else
	ent->mode = DEAD;

    if( ent->type_info->explosive ) {
	weapon_explode(ent, 50, ent->type_info->explosive);
    }


}


/* Returns a pointer to newly created entity, returns NULL on error */
entity *entity_alloc(void)
{
    entity *ent;
    
    if( (ent = (entity *)malloc( sizeof(entity) )) == NULL)
	perror("Malloc");

    return ent;

}






