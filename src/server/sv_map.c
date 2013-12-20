#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "world.h"
#include "sv_map.h"
#include "game.h"
#include "weapon.h"
#include "clients.h"
#include "sv_net.h"
#include "event.h"
#include "../common/entity_class_names.h"

#define MAXLINE 1024
#define MAXSPAWNS 32

typedef enum {
    SV_MAP_OTHER, /* Part of the map file that doesn't concern us */
    SV_MAP_OBJECTIVE, /* Game OBJECTIVE for the map */
    SV_MAP_EVENTMETA, /* Data about event data */
    SV_MAP_ENTITY
} sv_maplevel_t;

extern byte num_entity_types;
extern game_t game;
extern map_t *map;

static int spawns;
static spawn_t spawnpoint[MAXSPAWNS];

static int sv_read_map(char *filename);
static int read_objective(char *line);
static int read_entity(const char *line);

/* Changess to map_name. Returns 1 on success, 0 on failure but still ok to
   use the same map, or -1 on complete failure */
int sv_map_changelevel(char *filename, gamemode_t game_mode)
{
    map_t *old_map;
    int sv_map_data_changed = 0; 
    char map_name[NETMSG_STRLEN];

    old_map = map;
    strncpy(map_name, filename, NETMSG_STRLEN); /* overwritten in game_close */
    game_close(); /* Close current game */

    if( (game.mode = game_mode) >= NUM_GAME_MODES )
	game.mode = HOLYWAR; /* Default on error */

    if( (map = map_load(map_name, L_BASE | L_OBJECT | L_EVENT, CLIPMAP)) ) {
	if( sv_read_map(map_name) >= 0 ) {
	    sv_map_data_changed = 1;
	    sv_net_tell_clients_to_changelevel();
	    game_start();
	    if( old_map )
		map_close(&old_map);
	    return 1;
	} else {
	    printf("Error loading server data from %s\n", map_name);
	}
    } else
	printf("Load failed for map \"%s\"\n", map_name);

    /* Reload old map */
    if( old_map ) {
	printf("Reloading %s\n", game.map_name);
	map_close(&map);
	map = old_map;
	/* Do we need to reload the server part? */
	if( sv_map_data_changed && sv_read_map(game.map_name) < 0 )
	    return -1;
	game_start();
	return 0;
    } else
	return -1;

}


void spawn(entity *ent, int reset)
{
    int i;

    i = spawns * (float)rand() / RAND_MAX;
    ent->x = spawnpoint[i].x;
    ent->y = spawnpoint[i].y;
    ent->x_v = spawnpoint[i].x_v;
    ent->y_v = spawnpoint[i].y_v;
    ent->dir = spawnpoint[i].dir;
    ent->mode = ALIVE;
    ent->powerup = 0;
    memset(ent->powerup_expire, 0, sizeof(ent->powerup_expire));

    if( reset ) {
	ent->health = ent->type_info->health;
	weapon_reset(ent);
	memset(ent->ammo, 0, sizeof(ent->ammo));
    }

    entity_spawn_effect(ent);

}


/* Characters used at start of line to signify mode X ONLY */
static char gamemodechar[NUM_GAME_MODES] = "$%"; /* SAVETHEWORLD, HOLYWAR */

/* Read additional (server only) data from the map. */
static int sv_read_map(char *filename)
{
    char buf[MAXLINE], *line;
    FILE *file;
    int i, j;
    int entities, skipline;
    sv_maplevel_t map_section;

    /* Open the map file */
    if( !(file = open_data_file("maps", filename) ) ) {
	printf("%s: Couldn't open %s\n", __FILE__, filename);
	return -1;
    }

    strncpy(game.map_name, filename, NETMSG_STRLEN);

    /* Reset things loaded in sv_map */
    entities = 0;
    spawns = 0;
    entity_remove_all_non_clients();
    event_reset();
    map_section = SV_MAP_OTHER;

    /* Go through map file until we reach the headers for appropriate sections
       then handle each entity line by line */
    for( i=0 ; !feof(file) ; i++ ) {
	if( !fgets(buf, MAXLINE, file) )
	    break;
	if( buf[0] == '#' || buf[0] == '\n' || buf[0] == '\0' )
	    continue; /* skip line */

	line = buf;
	skipline = 0;
	for(j=0 ; j<NUM_GAME_MODES ; j++ ) {
	    if( buf[0] == gamemodechar[j] ) {
		if( game.mode == j )
		    line = buf + 1;/* Offset by 1 char, ie skip gamemodechar */
		else
		    skipline = 1; /* C needs a labelled continue! */
	    }
	}

	if( skipline )
	    continue;

	CHOMP(line);

	/* What map section are we at? */
	if( !strcasecmp("BASE", line) ||
	    !strcasecmp("OBJECT", line) ||
	    !strcasecmp("TOPLEVEL", line) )
	    map_section  = SV_MAP_OTHER;
	else if( !strcasecmp("OBJECTIVE", line) )
	    map_section = SV_MAP_OBJECTIVE;
	else if( !strcasecmp("EVENTMETA", line) )
	    map_section = SV_MAP_EVENTMETA;
	else if( !strcasecmp("ENTITY", line) )
	    map_section = SV_MAP_ENTITY;
	else {
	    switch( map_section ) {
	    case SV_MAP_OBJECTIVE:
		read_objective(line);
		break;
	    case SV_MAP_EVENTMETA:
		read_event(line);
		break;
	    case SV_MAP_ENTITY:
		switch( read_entity(line) ) {
		case 0:
		    printf("Error reading entity in %d:\"%s\"\n", i, line);
		    break;
		case 1:
		    entities++;
		    break;
		case 2:
		    spawns++;
		    break;
		}
		break;
	    case SV_MAP_OTHER:
	    default:
		break;
	    }
	}
    }

    fclose(file);
    event_setup_finish() ; /* Finish the event setup */

    if( map_section < SV_MAP_ENTITY ) {
	printf("No ENTITY section FOUND!\n");
	return 0;
    }

    printf("\"%s\": %d entities %d spawn-points.\n",
	   game.map_name, entities, spawns);
    return 1;

}


static char *game_objective_name[NUM_GAME_OBJECTIVES] = {
    "EXIT_LEVEL",
    "ELIMINATE_CLASS",
    "GAME_STAY_ALIVE"
};

static int read_objective(char *line)
{
    char *args;
    int i;

    if( !strncasecmp(line, "type", 4) ) {
	args = line + 5;
	for( i=0 ; i<NUM_GAME_OBJECTIVES ; i++ ) {
	    if( !strcasecmp(args, game_objective_name[i]) ) {
		game.objective = i;
	    }
	}
    } else if( !strncasecmp(line, "class", 5) ) {
	args = line + 6;
	for( i=0 ; i<NUM_ENT_CLASSES ; i++ ) {
	  if( !strcasecmp(args, ent_class_name[i]) ) {
	      game.class = i;
	  }
	} 
    } else if( !strncasecmp(line, "success_msg", 11) ) {
	args = line + 12;
	strncpy(game.success_msg, args, TEXTMESSAGE_STRLEN);
    } else if( !strncasecmp(line, "nextmap", 7) ) {
	args = line + 8;
	strncpy(game.next_map_name, args, NETMSG_STRLEN);
    } else {
	printf("Error parsing game objective from line = \"%s\"\n", line);
	return 0;
    }
    
    return 1;

}


extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];

/* Returns 0: Error, 1: Entity, 2: Spawn point */
static int read_entity(const char *line)
{
    char name[32];
    ent_type_t *et;
    entity *new;
    float x_v, y_v;
    int x, y;
    int speed;
    int i;
    byte dir;
    
    if( sscanf(line, "%s %d %d %d %d", name, &x, &y, &i, &speed) != 5 ) {
	return 0;
    }

    dir = i;

    x_v = sin_lookup[dir] * speed;
    y_v = -cos_lookup[dir] * speed;

    if( strcasecmp("SPAWN", name ) == 0 ) {
	spawnpoint[spawns].x = x;
	spawnpoint[spawns].y = y;
	spawnpoint[spawns].x_v = x_v;
	spawnpoint[spawns].y_v = y_v;
	spawnpoint[spawns].dir = dir;
	return 2;
    }

    /* Load monsters etc */
    for( i=0 ; i < num_entity_types ; i++ ) {
	if( (et = entity_type(i)) == NULL )
	    continue;
	if( strcasecmp( et->name, name ) == 0 ) {
	    if( et->class == VIRTUAL ) {
		printf("Not loading VIRTUAL entity \"%s\" in map\n", name);
		return 1;
	    }
	    /* Only load goodies and baddies in save the world game mode */
	    if( et->class == BADDIE || et->class == GOODIE ) {
		if( game.mode != SAVETHEWORLD )
		    return 1;
	    }
	    new = entity_new(i, x, y, x_v, y_v);
	    new->dir = dir;
	    if( world_check_entity_move( new, 0.0 ) == 0 ) {
		printf("entity (%d,%d) starts off clipping\n", x, y);
		entity_delete(new);
		return 0;
	    }
	    return 1;
	}
    }

    printf("BAD entity \"%s\" read!!\n", name);
    return 0;

}

