#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "world.h"
#include "sv_map.h"
#include "clients.h"
#include "sv_netmsg_send.h"
#include "event.h"
#include "game.h"
#include "weapon.h"

extern map_t *map;
extern game_t game;
extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];
extern byte num_entity_types;
extern server_t server;

/* Each event read in from the map (section EVENT) is identified by a symbol
   (printable character). Multiple map tiles (EVENTDATA) can have the same
   event value. event_symbol_table keeps track of these, which are used in
   read_event_data() to fill in the event map layer with the number of the
   event, that is looked up in the event_tab[] array. */
static char event_symbol_table[256];
static int events = 0;
static event_t *event_tab; /* malloc'd to size events */

static void event_update(event_t *event);
static void update_virtual_entity(event_t *event);

/* Delete all current events */
void event_reset(void)
{

    if( events ) {
	free(event_tab);
	events = 0;
    }
    memset(event_symbol_table, 0, 256);

}


static char *event_name[NUM_EVENTS] = {
    "NONE",
    "MESSAGE",
    "TELEPORT",
    "ENDLEVEL",
    "DAMAGE",
    "MAP_TARGET",
    "TRIGGER",
    "SET_OBJECTIVE",
    "OBJECTIVE_COMPLETE",
    "DOOR",
    "LOSEWEAPONS",
    "LOOKAT"
};

#define CAN_PASS 0
#define NO_PASS 1
#define DONT_MOVE 2

static int event_trigger_none(entity *ent, event_t *event)
{

    return 0;

}

static int event_trigger_message(entity *ent, event_t *event)
{

    if( ent->cl && event->data.message.text[0] ) {
	    sv_netmsg_send_gamemessage(ent->cl, event->data.message.text, 0);
    }

    return 0;
}

static int event_trigger_teleport(entity *ent, event_t *event)
{

    ent->x = event->data.teleport.x * TILE_W;
    ent->y = event->data.teleport.y * TILE_H;
    ent->dir = event->data.teleport.dir;
    ent->x_v = sin_lookup[ent->dir] * event->data.teleport.speed;
    ent->y_v = -cos_lookup[ent->dir] * event->data.teleport.speed;
    entity_spawn_effect(ent);
    return DONT_MOVE; /* We will handle new position, not calling functions */

}

static int event_trigger_damage(entity *ent, event_t *event)
{

    ent->health -= event->data.damage.damage;
    return 0;

}

static int event_trigger_endlevel(entity *ent, event_t *event)
{

    strncpy(game.next_map_name, event->data.endlevel.map, NETMSG_STRLEN);
    game.changelevel = 1;
    return 0;
}

static int event_trigger_map_target(entity *ent, event_t *event)
{

    if( ent->cl ) {
	sv_netmsg_send_map_target(ent->cl, event->data.map_target.active,
				  event->data.map_target.target);
	if( event->data.map_target.text[0] )
	    sv_netmsg_send_gamemessage(ent->cl, event->data.map_target.text,0);
    }

    return 0;

}

static int event_trigger_trigger(entity *ent, event_t *event)
{
    int change;
    event_t *targ;

    event->data.trigger.on = !event->data.trigger.on;
    if( event->data.trigger.target ) {
	targ = &event_tab[event->data.trigger.target];
	change = (event->data.trigger.on? 1:-1) * event->data.trigger.val;
	targ->value += change;
	event_update(targ);
    }

    if( event->data.trigger.on && event->data.trigger.text[0] )
	sv_netmsg_send_gamemessage(0, event->data.trigger.text, 1);

    return 0;

}

static int event_trigger_set_objective(entity *ent, event_t *event)
{

    sv_netmsg_send_objective(0, event->data.set_objective.text, 1);
    return 0;

}

static int event_trigger_objective_complete(entity *ent, event_t *event)
{

    game.objective_complete = 1;
    sv_netmsg_send_gamemessage(0, event->data.objective_complete.text, 1);
    return 0;

}


static int event_trigger_door(entity *ent, event_t *event)
{

    if( event->active ) {
        /* Check to see if virtual entity is still animating through change */
	if( event->virtual && event->virtual->mode != ALIVE )
	    return NO_PASS;
	else
	    return CAN_PASS;
    } else
	return NO_PASS;

}


static int event_trigger_loseweapons(entity *ent, event_t *event)
{

    weapon_reset(ent);
    memset(ent->ammo, 0, sizeof(ent->ammo));

    if( ent->cl && event->data.loseweapons.text[0] ) {
	sv_netmsg_send_gamemessage(ent->cl, event->data.loseweapons.text, 0);
    }

    return 0;

}


static int event_trigger_lookat(entity *ent, event_t *event)
{

    if( ent->cl && event->data.map_target.text[0] ) {
	sv_netmsg_send_gamemessage(ent->cl, event->data.lookat.text, 0);
    }

    ent->lookatdir = event->data.lookat.dir;
    ent->speed = 0;

    return 0;

}



static int (*event_trigger_handle[NUM_EVENTS])(entity *, event_t *) = {
    event_trigger_none,
    event_trigger_message,
    event_trigger_teleport,
    event_trigger_endlevel,
    event_trigger_damage,
    event_trigger_map_target,
    event_trigger_trigger,
    event_trigger_set_objective,
    event_trigger_objective_complete,
    event_trigger_door,
    event_trigger_loseweapons,
    event_trigger_lookat,
};


/* Time in msecs between triggers of an event */
#define TRIGGER_TIME 750

int event_trigger(entity *ent, byte id)
{
    event_t *event;
    int val;

    if( id < events ) {
	event = &event_tab[id];

	/* Doors block when inactive */
	if( event->active == 0 && event->type != EVENT_DOOR )
	    return 0;

	if( event->exclusive && ent->class != GOODIE )
	    return 0;

	if( event->type != EVENT_DOOR ) { /* Doors are special */
	    if( server.now - event->last_trigger >= TRIGGER_TIME )
		event->last_trigger = server.now;
	    else
		return 0; /* Not ready to be triggered */
	}

        val = event_trigger_handle[event->type](ent, event);

	if( event->type != EVENT_DOOR )
	    event->active = event->repeat;


	return val;

    }

    return 0;

}


typedef struct tmp_node_struct {
    struct tmp_node_struct *next;
    event_t event;
} tmp_node;

tmp_node *event_root, *event_tail;

static event_t *event_new(void)
{
    tmp_node *ptr;

    if( (ptr = (tmp_node *)malloc( sizeof(tmp_node) )) == NULL ) {
	perror("malloc");
	ERR_QUIT("Allocating new tmp node for an event", -1);
    }

    /* Add it to the list */
    if( event_root == NULL )
	event_root = ptr; /* The new event_root entity */

    if( event_tail != NULL )
	event_tail->next = ptr;

    event_tail = ptr;
    event_tail->next = NULL; /* end of list */
    ptr->event.good = 0; /* Not good by default */

    events++;

    return &ptr->event;

}


/* Setup the event layer to point to the right events and fill in trigger events */
int event_setup_finish(void)
{
    tmp_node *ptr, *next;
    int i;
    char ep;

    if( events <= 0 )
	return 0; /* No events */

    events++; /* We want an EVENT_NONE at the start */

    if( (event_tab = malloc( sizeof(event_t) * events )) == NULL )
	ERR_QUIT("Error allocating event_tab[] array", -1);

    event_tab[0].type = EVENT_NONE;
    event_tab[0].active = 0;
    event_tab[0].repeat = 0;
    event_tab[0].visible = 0;

    events = 1; /* So far we have 1 good event (EVENT_NONE) */
    ptr = event_root;
    for( i=1 ; ptr != NULL ; i++ ) {
	next = ptr->next;
	if( ptr->event.good ) {
	    event_tab[i] = ptr->event;
	    events++;
	} else {
	    printf("Dropping event type %s\n", event_name[ptr->event.type]);
	    event_tab[i].type = EVENT_NONE;
	}
	free(ptr);
	ptr = next;
    }

    event_root = NULL;
    event_tail = NULL;

    for(i=0 ; i<map->width * map->height ; i++ )
	map->event[i] = event_symbol_table[ (int)map->event[i] ];

    for(i = 0; i < events; i++) {
	if( event_tab[i].type == EVENT_TRIGGER ) {
	    ep = event_symbol_table[event_tab[i].data.trigger.letter];
	    event_tab[i].data.trigger.target = ep;
	    if( ep ) {
		if( event_tab[i].data.trigger.on ) { /* Do initial trigger */
		    event_t *targ;
		    targ = &event_tab[(int)ep];
		    targ->value += event_tab[i].data.trigger.val;
		    event_update(targ);
		}
	    } else {
		printf("No such event \"%c\" for trigger\n",
		       event_tab[i].data.trigger.letter);
	    }
	}
    }

    return 1;

}


/* Draw the events for specified client */
void event_draw(client_t *cl)
{
    animation_t *ani;
    ent_type_t *type;
    event_t *event;
    netmsg msg;
    int x, y;
    int x_start, y_start;
    int x_max, y_max;
    int mode;
    byte e;

    if( map->event == NULL )
	return;

    x_start = cl->screenpos.x/TILE_W;
    y_start = cl->screenpos.y/TILE_H;

    x_max = MIN( (cl->screenpos.x + cl->view_w)/TILE_W + 1, map->width );
    y_max = MIN( (cl->screenpos.y + cl->view_h)/TILE_H + 1, map->height );

    for( y=y_start ; y < y_max ; y++ ) {
	for( x=x_start ; x < x_max ; x++ ) {
	    if( (e = map->event[ y*map->width + x ]) ) {
		if( e >= events )
		    continue;
		event = &event_tab[e];
		if( !event->visible )
		    continue;

		/* Doors "activeness" is different */
		if( event->active == 0 && event->type != EVENT_DOOR )
		    continue;

		if( event->virtual ) {
		    update_virtual_entity(event);
		    type = event->virtual->type_info;
		    assert( type != NULL );

		    /* Make triggers act correctly */
		    if( event->type == EVENT_TRIGGER )
			mode = event->data.trigger.on? ALIVE : DEAD;
		    else
			mode = event->virtual->mode;
		    ani = type->animation[mode];
		    
		    if( ani != NULL ) {
			msg = entity_to_netmsg(event->virtual);
			msg.type = NETMSG_ENTITY;
			msg.entity.x = x * TILE_W;
			msg.entity.y = y * TILE_H;
			msg.entity.mode = mode;
			net_put_message(cl->nc, msg);
		    }

		} else {  /* If no virtual entity maybe some pixel effects */

		    msg.type = NETMSG_PARTICLES;
		    msg.particles.effect = P_TELEPORT;

		    msg.particles.length = TILE_W/2;
		    msg.particles.x = x * TILE_W + TILE_W/2;
		    msg.particles.y = y * TILE_H + TILE_H/2;

		    if( event_tab[e].type == EVENT_TELEPORT ) {
			msg.particles.color1 = COL_GRAY;
			msg.particles.color2 = COL_YELLOW;
			net_put_message(cl->nc, msg);
		    } else if( event_tab[e].type == EVENT_ENDLEVEL ) {
			msg.particles.color1 = COL_PINK;
			msg.particles.color2 = COL_PURPLE;
			net_put_message(cl->nc, msg);
		    }
		}
	    }
	}
    }

}





static void event_new_none(event_t *event, const char *args)
{

    event->good = 1;

}


static void event_new_message(event_t *event, const char *args)
{

    if( args ) {
	strncpy(event->data.message.text, args, TEXTMESSAGE_STRLEN);
	event->good = 1;
    }

}


static void event_new_teleport(event_t *event, const char *args)
{
    int x, y, dir, speed;

    if( sscanf(args, "%d %d %d %d", &x, &y, &dir, &speed) == 4 ) {
	event->data.teleport.x = x;
	event->data.teleport.y = y;
	event->data.teleport.dir = dir;
	event->data.teleport.speed = speed;
	event->good = 1;
    } else {
	printf("Error parsing teleport event, args = \"%s\"\n", args);
    }

}


static void event_new_endlevel(event_t *event, const char *args)
{
    if( args ) {
	strncpy(event->data.endlevel.map, args, NETMSG_STRLEN);
	event->good = 1;
    }
}

static void event_new_damage(event_t *event, const char *args)
{
    int damage;

    if( sscanf(args, "%d", &damage) == 1 ) {
	event->data.damage.damage = damage;
	event->good = 1;
    } else {
	printf("Error parsing damage object\n");
    }

}


static void event_new_map_target(event_t *event, const char *args)
{
    int i, len;

    if( sscanf(args, "%d %d %d",
	       (int *)&event->data.map_target.active,
	       (int *)&event->data.map_target.target.x,
	       (int *)&event->data.map_target.target.y) != 3 ) {
	printf("Error parsing map target event\n");
	return;
    }

    len = strlen(args);
    for( i = 0 ; isdigit(args[i]) || isspace(args[i]) ; i++ )
	if( i >= len )
	    return;

    strncpy(event->data.map_target.text, args + i, TEXTMESSAGE_STRLEN);
    event->good = 1;

}


static void event_new_trigger(event_t *event, const char *args)
{
    char letter, on;
    int i, len, val;

    if( sscanf(args, "%c %c %d", &letter, &on, &val) != 3 ) {
	printf("No letter, ON/OFF flag or val in trigger \"%s\"\n",args);
	return;
    }

    event->data.trigger.letter = letter;
    event->data.trigger.on = (on == 'y');
    event->data.trigger.val = val;

    len = strlen(args);
    for( i = 5 ; isspace(args[i]) || isdigit(args[i]) ; i++ )
	if( i >= len )
	    return;

    strncpy(event->data.trigger.text, args + i, TEXTMESSAGE_STRLEN);
    event->good = 1;

}

static void event_new_set_objective(event_t *event, const char *args)
{
    strncpy(event->data.set_objective.text, args, TEXTMESSAGE_STRLEN);
    event->good = 1;
}


static void event_new_objective_complete(event_t *event, const char *args)
{
    strncpy(event->data.objective_complete.text, args, TEXTMESSAGE_STRLEN);
    event->good = 1;
}


static void event_new_door(event_t *event, const char *args)
{
    event->good = 1;
}


static void event_new_loseweapons(event_t *event, const char *args)
{
    if( args ) {
	strncpy(event->data.loseweapons.text, args, TEXTMESSAGE_STRLEN);
	event->good = 1;
    }
}

static void event_new_lookat(event_t *event, const char *args)
{
    int i, len, dir;

    if( args ) {
	
	if( sscanf(args, "%d", &dir) != 1 ) {
	    printf("No lookat dir found.\n");
	    return;
	}

	event->data.lookat.dir = dir;

	len = strlen(args);
	for( i = 0 ; isspace(args[i]) || isdigit(args[i]) ; i++ )
	    if( i >= len )
		return;
    
	strncpy(event->data.lookat.text, args + i, TEXTMESSAGE_STRLEN);
	event->good = 1;
    }
}


static void (*event_handle[NUM_EVENTS])(event_t *event, const char *args) = {
    event_new_none,
    event_new_message,
    event_new_teleport,
    event_new_endlevel,
    event_new_damage,
    event_new_map_target,
    event_new_trigger,
    event_new_set_objective,
    event_new_objective_complete,
    event_new_door,
    event_new_loseweapons,
    event_new_lookat
};


/*
  We are passed <ENTITY_NAME> XXXX-SOME-DATA-XXXX.
  return the entity type that matches ENTITY_NAME (or -1 in error)
  set len to be the position where the XXX-SOME-DATA-XXX data starts
*/

int event_get_virtual_entity(const char *args, int *len)
{
    ent_type_t *et;
    char *p;
    int i;

    if( (p = strchr(args, '>')) == NULL ) {
	printf("Error: '>' not found searching for virtual entity\n");
	return -1;
    }

    *len = p + 1 - args;
    args++; /* Step past "<" */

    for( i=0 ; i < num_entity_types ; i++ ) {
	if( (et = entity_type(i)) == NULL )
	    continue;
	/* *len - 2 == length from args to '>' */
	if( strncasecmp( et->name, args, *len - 2) == 0 ) {
	    /* printf("event_get_virtual_entity: MATCH(%s)\n", et->name); */
	    return i;
	}
    }

    printf("No entity type found in \"%s\"\n", args);
    return -1;

}


static void event_update(event_t *event)
{
    animation_t *ani;
    int valid = 0;

    /* Check to see if value will unlock event (if it has correct relationship
       with key)*/
    if( event->key ) {
	switch( event->vk_cmp ) {
	case VK_EQ:
	    valid = (event->value == event->key);
	    break;
	case VK_GT:
	    valid = (event->value > event->key);
	    break;
	}

	/* Change states.... */
	if( event->active && !valid ) {
	    if( event->virtual ) {
		if( event->virtual->type_info->animation[DYING] ) {
		    event->direction = 1;
		    event->virtual->frame = 0;
		    event->virtual->last_frame = server.now;
		    event->virtual->mode = DYING;
		} else {
		    event->virtual->mode = DEAD;
		}
	    }
	    event->active = 0;
	}

	if( !event->active && valid ) {
	    if( event->virtual ) {
		if( (ani = event->virtual->type_info->animation[DYING]) ) {
		    event->virtual->frame = ani->frames - 1;
		    event->virtual->last_frame = server.now;
		    event->direction = -1;
		    event->virtual->mode = DYING;
		} else {
		    event->virtual->mode = ALIVE;
		}
	    }
	    event->active = 1;
	}
    }

}



static void update_virtual_entity(event_t *event)
{
    animation_t *ani;
    entity *ent;
    int frames;

    ent = event->virtual;
    if( (ani = ent->type_info->animation[ent->mode]) == NULL )
	return; /* Nothing shown for this state...... */

    if( ani->images > 1 ) {
	if( ani->framelen )
	    frames = (server.now - ent->last_frame) / ani->framelen;
	else
	    frames = 0;

	if( frames > 0 ) {
	    ent->last_frame = server.now;
	    ent->frame += event->direction * frames;
	    if( ent->frame >= ani->frames ) {
		if( ent->mode == DYING ) {
		    if( event->direction == 1 ) { /*  1 = ALIVE --> DEAD */
			ent->mode = DEAD;
			event->active = 0;
		    } else {                      /* -1 =  DEAD --> ALIVE */
			ent->mode = ALIVE;
			event->active = 1;
		    }
		}
		event->direction = 1;
		ent->frame = 0;
	    }

	}
	ent->img = ani->order[ent->frame];
    }


}


void read_event(const char *line)
{
    event_t *event;
    ent_type_t *et;
    int i, len, ent_type, event_type, key;
    val_key_cmp_t vk_cmp = VK_EQ;
    const char *args;
    char c, a, r, v, e; /* id character, active, repeat, visible, exclusive */

    event_type = -1;
    for(i=0 ; i<NUM_EVENTS; i++ ) {
	len = strlen(event_name[i]);
	if( !strncasecmp(line, event_name[i], len) ) {
	    args = line + len + 1; /* Args are 1 space after the event name */
	    /* Get symbol representation for event */
	    if( sscanf(args, "%c", &c) != 1 ) {
		printf("Error reading EVENT symbol \"%s\"\n", line);
		break;
	    }
	    args += 2; /* Move past the character */

	    /* Get settings ACTIVE REPEAT VISIBLE EXCLUSIVE flags */
	    if( sscanf(args, "%c %c %c %c", &a, &r, &v, &e) != 4 ) {
		printf("Error reading 4 EVENT flags from \"%s\"\n", line);
		break;
	    }
	    args += 8; /* Move past the characters */

	    /* Set Key for event */
	    if( !strncmp(args, "KEY", 3) ) {
		args += 3;
		if( *args == '>' )
		    vk_cmp = VK_GT; /* Greater than */
		else if( *args == '=' )
		    vk_cmp = VK_EQ; /* Equal to */
		else {
		    printf("ERROR reading vk_cmp in line = \"%s\"\n", line);
		    break;
		}

		args++;
		if( sscanf(args, "%d", &key) != 1 ) {
		    printf("ERROR reading key value in line = \"%s\"\n", line);
		    break;
		}

		while( !isspace(*args++) ); /* Get to start of next data */

	    }


	    ent_type = -1;
	    /* There is a VIRTUAL entity in brackets, ie <ENT_TYPE> */
	    if( *args == '<' ) {
		int len = 0;
		ent_type = event_get_virtual_entity(args, &len);
		args += len + 1; /* Step past <.*> and 1 space */
	    }

	    /* Make a new event, add it's symbol to the symbol table and
	       pass the rest of the line off to the individual routines to
	       fill in the rest of the event */
	    event = event_new();
	    event->type = i;
	    event->vk_cmp = vk_cmp;
	    event->key = key;
	    event->last_trigger = 0;
	    event->direction = 1; /* Play animation forwards */
	    event->active    = (a == 'y');
	    event->repeat    = (r == 'y');
	    event->visible   = (v == 'y');
	    event->exclusive = (e == 'y');
	    event_symbol_table[(int)c] = events;
	    event_handle[i](event, args);

	    if( event->active )
		event->value = event->key;
	    else
		event->value = 0;

	    if( ent_type >= 0 && event->type != EVENT_NONE ) {
		if( (event->virtual = entity_alloc()) != NULL ) {
		    memset( event->virtual, 0, sizeof( entity ) );
		    if( (et = entity_type(ent_type)) != NULL )
			event->virtual->type_info = et;
		    event->virtual->type = ent_type;
		    event->virtual->mode = event->active? ALIVE : DEAD;
		    event->virtual->last_frame = server.now;
		}
	    } else
		event->virtual = NULL;
	    break;
	}
    }

}


void event_close(void)
{
    int i;

    if( events <= 0 )
	return;

    printf("Free'ing %d events.\n", events);

    for( i = 0 ; i < events ; i++ ) {
	if( event_tab[i].virtual )
	    free(event_tab[i].virtual);
    }

    free(event_tab);

}
