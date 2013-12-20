#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "xtux.h"
#include "version.h"
#include "entity_class_names.h" /* Static array of entity class names */

#define ENTLIST "entities"

static ent_type_t **entity_type_table; /*  Size = num_entity_types */
static byte num_entity_types;

static ent_type_t *et_root;
static ent_type_t *et_tail;

static ent_type_t *create_entity_type(void);
static void entity_type_handle_line(char *line, ent_type_t *ent, int line_num);


/* Returns the number of entity types (MAX 256) */
byte entity_type_init(void)
{
    char line[MAXLINE];
    ent_type_t *ent;
    FILE *file;
    int i, line_num;
    
    num_entity_types = 0;
    ent = NULL;

    if( !(file = open_data_file(NULL, ENTLIST)) )
	return 0;

    for(line_num = 0 ; !feof( file ) && num_entity_types < 256 ; line_num++) {
	fgets( line, MAXLINE, file );
	CHOMP(line); /* Lose the \n */
	if( line[0] == '#' || line[0] == '\0' )
	    continue;
	if( line[0] == '[' ) {
	    if( (ent = create_entity_type()) == NULL)
		return 0;
	    num_entity_types++;
	    for( i=1 ; line[i] != ']' && i < 32 ; i++ )
		ent->name[ i-1 ] = line[i];
	    ent->name[i-1] = '\0';
	} else if( num_entity_types > 0 && ent ) {
	    entity_type_handle_line(line, ent, line_num);
	} else {
	    printf("%s: Ignoring line %d = \"%s\"\n",
		   __FILE__, line_num, line);
	}
    }


    i = sizeof(ent_type_t *) * num_entity_types;
    if( (entity_type_table = (ent_type_t **)malloc(i)) == NULL ) {
	printf("Error allocating entity type table");
	return 0;
    }

    ent = et_root;
    for( i=0 ; i < num_entity_types && ent != NULL ; i++ ) {
	entity_type_table[i] = ent;
	ent = ent->next;
    }

    return num_entity_types;

}


/* Just a convenience routine so we don't have to type out a massive array
   entry all the time */
ent_type_t *entity_type(byte type)
{
    if( type >= num_entity_types ) {
	printf("Error!, no entity of type %u\n", (unsigned int)type);
	return NULL;
    }
    
    return entity_type_table[ type ];

}


byte entity_types(void)
{
    return num_entity_types;
}


animation_t *entity_type_animation(ent_type_t *et, int mode)
{
    animation_t *ani;

    if( (ani = et->animation[ mode ] ) == NULL ) {
	/* Plan B, use Alive animation */
	if( (ani = et->animation[ ALIVE ]) == NULL ) {
	    printf("%s: Animation modes %d & ALIVE undefined for %s\n",
		   __FILE__, mode, et->name);
	    ERR_QUIT("No ALIVE animation. Fix your \"entity\" file!", -1);
	}
    }

    return ani;

}



/* Returns entity type for name "name" -1 on error */
int match_entity_type(char *name)
{
    ent_type_t *et;
    int i;
    
    for( i=0 ; i < num_entity_types ; i++) {
	if( (et = entity_type(i)) == NULL )
	    break;
	if( !strcasecmp(et->name, name) )
	    return i;
    }

    printf("NO MATCH for entity of  name \"%s\"!\n", name);
    return -1;

}


/* Returns a pointer to newly created ent_type_t, returns NULL on error */
static ent_type_t *entity_type_alloc(void)
{
    ent_type_t *ent;
    
    if( (ent = (ent_type_t *)malloc(sizeof(ent_type_t))) == NULL)
	perror("Malloc");
    
    return ent;

}


static ent_type_t *create_entity_type(void)
{
    ent_type_t *ent;

    /* Create ent_type_t */
    if( (ent = entity_type_alloc()) == NULL) {
	printf("Error allocating new entity type in create_entity_type()!\n");
	return NULL;
    }

    memset(ent, 0, sizeof(*ent));
 /* Defaults */
    ent->health = 1;
    ent->cliplevel = GROUNDCLIP;
    ent->explosive = 0;

    /* Add it to the list */
    if( et_root == NULL )
	et_root = ent;

    if( et_tail != NULL )
	et_tail->next = ent;

    et_tail = ent;
    ent->next = NULL; /* Sentinel at end of list. */

    return ent;

}


static int handle_str_str(ent_type_t *ent, char *str, char *str1);
static int handle_str_int(ent_type_t *ent, char *str, int arg1);
static int handle_order(ent_type_t *, char *, int v0, int v1, int v2, int v3);

static void entity_type_handle_line(char *line, ent_type_t *ent, int line_num)
{
    char str[32], str1[32];
    int v0, v1, v2, v3; /* Max of 4 values per line */
    int error;

    error = 0;

    if( !strncmp(line, "item_pickup_str ", 16) ) {
	line += 16;
	if( (ent->item_pickup_str = (char *)strdup(line)) == NULL ) {
	    perror("malloc");
	    error = 1;
	}
    } else if( !strncmp(line, "item_announce_str ", 18) ) {
	line += 18;
	/* Has format "%s picked up XXXXX" */
	if( str_format_count(line, 's') == 1 ) {
	    if( (ent->item_announce_str = (char *)strdup(line)) == NULL ) {
		perror("malloc");
		error = 1;
	    }
	} else {
	    printf("item announce string \"%s\" not valid!\n", line);
	    error = 1;
	}
    } else if( sscanf(line, "%s %d %d %d %d", str, &v0, &v1, &v2, &v3) == 5 ) {
	/* Only order has format string int int int int */
	error = handle_order(ent, str, v0, v1, v2, v3);
    } else if( sscanf(line, "%s %d", str, &v1) == 2 ) {
	error = handle_str_int(ent, str, v1);
    } else if( sscanf(line, "%s %s", str, str1) == 2 ) {
	error = handle_str_str(ent, str, str1);
    } else
	error = 1;
    
    if( error )
	printf("***** Error parsing line %d: \"%s\" in entity %s *****\n",
	       line_num, line, ent->name);

}


/* Returns a pointer to newly created ent_type_t, returns NULL on error */
static animation_t *animation_alloc(void)
{
    animation_t *ani;

    if( (ani = (animation_t *)malloc( sizeof(animation_t) )) == NULL ) {
	perror("Malloc");
	return NULL;
    }

    /* Defaults for each entity type */
	memset(ani, 0, sizeof(*ani));
    ani->images = 1;
    ani->frames = 1;
    ani->directions = 8;
    memset(ani->order, 0, 4);
    
    return ani;

}


static char *ent_mode[NUM_ENT_MODES] = {
    "LIMBO",
    "DEAD",
    "DYING",
    "GIBBED",
    "FROZEN",
    "ALIVE",
    "PATROL",
    "FIDGETING",
    "SLEEP",
    "ATTACK",
    "ENLIGHTENED",
    "COMEDOWN"
};


/* Returns the mode number that matches string with "MODE_word" */
static int match_mode(ent_type_t *ent, char *str, char *word)
{
    char buf[32];
    int i;

    for( i = 0 ; i < NUM_ENT_MODES ; i++ ) {
	snprintf( buf, 32, "%s_%s", ent_mode[ i ], word);
	/* printf("\n\n---- Looking for %s we have %s\n", str, buf); */
	if( !strcasecmp( buf, str ) ) {
	    if( ent->animation[i] == NULL ) {
		ent->animation[i] = animation_alloc();
	    }
	    return i;
	}
    }

    return -1;

}


static int handle_order(ent_type_t *ent, char *str, int a, int b, int c, int d)
{
    int mode;

    if( (mode = match_mode(ent, str, "order")) < 0 ) {
	printf("Bad mode in handle_order!\n");
	return mode;
    }

    ent->animation[mode]->order[0] = a;
    ent->animation[mode]->order[1] = b;
    ent->animation[mode]->order[2] = c;
    ent->animation[mode]->order[3] = d;
    return 0; /* Success */

}


static int handle_str_int(ent_type_t *ent, char *str, int arg1)
{
    int mode;

    if( !strcasecmp("health", str) ) {
	ent->health = arg1;
	return 0;
    } else if( !strcasecmp("speed", str) ) {
	ent->speed = arg1;
	return 0;
    } else if( !strcasecmp("width", str) ) {
	ent->width = arg1;
	return 0;
    } else if( !strcasecmp("height", str) ) {
	ent->height = arg1;
	return 0;
    } else if( !strcasecmp("accel", str) ) {
	ent->accel = arg1;
	return 0;
    } else if( !strcasecmp("draw_weapon", str) ) {
	ent->draw_weapon = arg1;
	return 0;
    } else if( !strcasecmp("touchdamage", str) ) {
	ent->touchdamage = arg1;
	return 0;
    } else if( !strcasecmp("explosive", str) ) {
	ent->explosive = arg1;
	return 0;
    } else if( !strcasecmp("bleeder", str) ) {
	ent->bleeder = arg1;
	return 0;
    } else if( !strcasecmp("dripping", str) ) {
	ent->dripping = arg1;
	return 0;
    } else if( !strcasecmp("drip_time", str) ) {
	ent->drip_time = arg1;
	return 0;
    } else if( !strcasecmp("drip1", str) ) {
	ent->drip1 = arg1;
	return 0;
    } else if( !strcasecmp("drip2", str) ) {
	ent->drip2 = arg1;
	return 0;
    } else if( !strcasecmp("sight", str) ) {
	ent->sight = arg1;
	return 0;
    } else if( !strcasecmp("item_value", str) ) {
	ent->item_value = arg1;
	return 0;
    } else if( !strcasecmp("item_respawn_time", str) ) {
	ent->item_respawn_time = arg1 * M_SEC;
	return 0;
    }

    /* Animation stuff */
    if( (mode = match_mode(ent, str, "framelen")) > 0 ) {
	ent->animation[mode]->framelen = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "stationary_ani")) > 0 ) {
	ent->animation[mode]->stationary_ani = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "draw_hand")) > 0 ) {
	ent->animation[mode]->draw_hand = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "img_w")) > 0 ) {
	ent->animation[mode]->img_w = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "img_h")) > 0 ) {
	ent->animation[mode]->img_h = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "arm_x")) > 0 ) {
	ent->animation[mode]->arm_x = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "arm_y")) > 0 ) {
	ent->animation[mode]->arm_y = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "images")) > 0 ) {
	ent->animation[mode]->images = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "dir")) > 0 ) {
	ent->animation[mode]->directions = arg1;
	return 0;
    }

    if( (mode = match_mode(ent, str, "frames")) > 0 ) {
	ent->animation[mode]->frames = arg1;
	return 0;
    }

    return -1;

}


static char *ai_mode_name[NUM_AI_MODES] = {
    "NONE",
    "FIGHT",
    "FLEE",
    "SEEK",
    "ROTATE"
};


static char *item_action_name[NUM_ITEM_ACTIONS] = {
    "HEAL",
    "FULLHEAL",
    "GIVEWEAPON",
    "GIVEAMMO",
    "GIVEKEY",
    "POWERUP"
};

static char *item_powerup_name[NUM_POWERUPS] = {
    "RANDOM",
    "HALF_AMMO_USAGE",
    "DOUBLE_FIRE_RATE",
    "WOUND",
    "RESISTANCE",
    "FROZEN",
    "INVISIBILITY",
    "E",
    "FIXED_WEAPON"
};


extern char *cliplevel_name[];

static int handle_str_str(ent_type_t *ent, char *str, char *str1)
{
    int i;
    int mode;

    /* The entities weapon, or weapon it gives you CAN'T be worked out until
       the weapon config file has been read to match the names */
    if( !strcasecmp("weapon", str) || !strcasecmp("item_weapon", str) ) {
	strncpy(ent->weapon_name, str1, 16);
	return 0;
    }


    if( !strcasecmp("class", str) ) {
	for( i=0 ; i<NUM_ENT_CLASSES ; i++ ) {
	    if( !strcasecmp( ent_class_name[i], str1 ) ) {
		ent->class = i;
		return 0;
	    }
	}
	printf("Entity class %s not found!\n", str1);
	return -1;
    } else if( !strcasecmp("item_action", str) ) {
	for( i=0 ; i<NUM_ITEM_ACTIONS ; i++ ) {
	    if( !strcasecmp( item_action_name[i], str1 ) ) {
		ent->item_action = i;
		return 0;
	    }
	}
	printf("Item action %s not found!\n", str1);
	return -1;
    } else if( !strcasecmp("item_powerup", str) ) {
	for( i=0 ; i<NUM_POWERUPS ; i++ ) {
	    if( !strcasecmp( item_powerup_name[i], str1 ) ) {
		ent->item_powerup = i;
		return 0;
	    }
	}
	printf("Item powerup %s not found!\n", str1);
	return -1;
    } else if( !strcasecmp("cliplevel", str) ) {
	for( i=0 ; i<NUM_CLIPLEVELS ; i++ ) {
	    if( !strcasecmp(cliplevel_name[i], str1) ) {
		ent->cliplevel = i;
		return 0;
	    }
	}
	printf("Clip level %s not found!\n", str1);
	return -1;
    } else if( !strcasecmp("ai", str) ) {
	for( i=0 ; i<NUM_AI_MODES ; i++ ) {
	    if( !strcasecmp(ai_mode_name[i], str1) ) {
		ent->ai = i;
		return 0;
	    }
	}
	printf("AI type %s not found!\n", str1);
	return -1;
    } else if( !strcasecmp("item_ammo_type", str) ) {
	if( (ent->item_type = match_ammo_type(str1)) > 0 )
	    return 0;
	else
	    return -1;
    }
    
    if( (mode = match_mode(ent, str, "pixmap")) < 0 )
	return mode;
    else {
	strncpy( ent->animation[mode]->pixmap, str1, 32 );
	return 0;
    }
    
    printf("Couldn't match %s to any mode\n", str);
    return -1;

}

