#include <stdio.h>
#include <stdlib.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "item.h"
#include "game.h"
#include "clients.h"
#include "sv_netmsg_send.h"

extern byte num_entity_types;
extern byte num_weapon_types;
extern server_t server;
extern game_t game;

static int random_powerup(void);

void item_collision(entity *item, entity *ent)
{
    char buf[TEXTMESSAGE_STRLEN];
    ent_type_t *it, *et;
    weap_type_t *wt;
    int new_health, powerup;
    int remove_item = 1; /* Default, set to 0 to stop */

    it = item->type_info;
    et = ent->type_info;

    /* Shouldn't happen in theory, in theory. */
    if( it->class != ITEM ) {
	printf("Error: item_collision called for a non-item!\n");
	return;
    }

    switch( it->item_action ) {
    case HEAL:
	new_health = ent->health + it->item_value;
	if( new_health > et->health )
	    new_health = et->health; /* keep new_health <= et->health */
	if( ent->health == new_health ) { /* No change, don't pickup */
	    remove_item = 0;
	    break;
	}
	ent->health = new_health;
	break;
    case FULLHEAL:
	if( ent->health == et->health ) { /* Already at max */
	    remove_item = 0;
	    break;
	}
	ent->health = et->health; /* Back to maximum */
	break;
    case GIVEWEAPON:
	if( it->item_value >= num_weapon_types ) {
	    printf("ERROR, item has value %d, which is >= num_weapon_types!\n",
		   it->item_value);
	} else if( game.mode == HOLYWAR && ent->has_weapon[it->item_type] ) {
	    remove_item = 0;
	} else {
	    wt = weapon_type(it->item_type);
	    ent->has_weapon[ it->item_type ] = 1;
	    ent->ammo[wt->ammo_type] += wt->initial_ammo;
	    /* switch to the weapon if you don't already have one. */
	    if( ent->weapon == 0 )
		ent->weapon = it->item_type;
	}
	break;
    case GIVEAMMO:
	ent->ammo[it->item_type] += it->item_value; 
	break;
    case POWERUP:
	if( it->item_powerup == PU_RANDOM ) {
	    powerup = random_powerup();
	    if( powerup < 0 || (it = entity_type(powerup)) == NULL ) {
		printf("Error, powerup < 0\n");
		break;
	    }
	}

	if( it->item_powerup > 0 && it->item_powerup < NUM_POWERUPS ) {
	    remove_item = 1;
	    ent->powerup |= (1<<it->item_powerup);
	    if( ent->powerup_expire[it->item_powerup] ==  0 ) {
		ent->powerup_expire[it->item_powerup] = server.now;
	    }
	    ent->powerup_expire[it->item_powerup] += it->item_value*M_SEC;
	    /* Specific item powers */
	    switch( it->item_powerup ) {
	    case PU_FROZEN:
		ent->mode = FROZEN;
		break;
	    case PU_E:
		ent->mode = ENLIGHTENED;
		ent->speed = et->speed * 1.5;
		break;
	    case PU_FIXED_WEAPON:
		ent->weapon = ent->type_info->weapon;
		break;
	    default:
		break;
	    }
	} else {
	    printf("ERROR! Powerup %d out of range.\n", it->item_powerup);
	}
	break;
    default:
	printf("ITEM ACTION %d NOT HANDLED!\n", it->item_action);
    }

    if( remove_item ) {
	if( game.mode == HOLYWAR && it->item_respawn_time ) {
	    item->mode = LIMBO;
	    item->respawn_time = server.now + it->item_respawn_time;
	} else
	    item->health = 0; /* Remove item for good */

	/* Send item pickup announcement to EVERYONE */
	if( it->item_announce_str ) {
	    snprintf(buf, TEXTMESSAGE_STRLEN, it->item_announce_str,ent->name);
	    sv_netmsg_send_gamemessage(0, buf, 1);
	}

	/* Send pickup message to client */
	if( it->item_pickup_str ) {
	    if( ent->cl )
		sv_netmsg_send_gamemessage(ent->cl, it->item_pickup_str, 0);
	}
    }

}



/* Returns the entity type of a random powerup entity, or -1 on failure */
static int random_powerup(void)
{
    ent_type_t *et;
    int i, powerup;

    /* Random powerup between 1 and NUM_POWERUPS */
    powerup = 1 + (int)((NUM_POWERUPS-1.0)*rand()/(RAND_MAX+1.0));

    for(i=0 ; i<num_entity_types ; i++ ) {
	if( (et = entity_type(i)) == NULL )
	    return -1;
	if( et->class == ITEM && et->item_powerup == powerup )
	    return i;
    }

    printf("No entity found with powerup type %d\n", powerup);
    return -1;

}
