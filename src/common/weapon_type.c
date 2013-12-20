#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xtux.h"
#include "version.h"
#include "ammo.h" /* static array of ammunition names */

#define WEAPFILE "weapons"

static weap_type_t **weapon_type_table; /*  Size = num_weapon_types */
static byte num_weapon_types;

static weap_type_t *wt_root;
static weap_type_t *wt_tail;

static weap_type_t *create_weapon_type(void);
static void weap_type_handle_line(char *line, weap_type_t *weap);


/* Returns the number of weapon types (MAX 255) */
byte weapon_type_init(void)
{
    char line[MAXLINE];
    weap_type_t *weap;
    FILE *file;
    int i;
    
    num_weapon_types = 0;

    if( !(file = open_data_file(NULL, WEAPFILE)) )
	return 0;

    /* Create initial NONE weapon type */
    if( (weap = create_weapon_type()) ) {
	num_weapon_types++;
	strncpy(weap->name, "NONE", 32);
    } else {
	return 0;
    }

    while( !feof( file ) && num_weapon_types < 256 ) {
	fgets( line, MAXLINE, file );
	CHOMP(line); /* Lose the \n */
	if( line[0] == '#' || line[0] == '\0' )
	    continue;
	if( line[0] == '[' ) {
	    if( (weap = create_weapon_type()) == NULL )
		return 0;
	    num_weapon_types++;
	    for( i=1 ; line[i] != ']' && i < 32 ; i++ )
		weap->name[ i-1 ] = line[i];
	    weap->name[i-1] = '\0';
	} else if( num_weapon_types > 1 ) {
	    weap_type_handle_line(line, weap);
	} else {
	    printf("%s: Ignoring line \"%s\"\n", __FILE__, line);
	}
    }


    i = sizeof(weap_type_t *) * num_weapon_types;
    if( (weapon_type_table = (weap_type_t **)malloc(i)) == NULL ) {
	printf("Error allocating weapon_type_table\n");
	return 0;
    }

    weap = wt_root;
    for( i=0 ; i < num_weapon_types && weap != NULL ; i++ ) {
	weapon_type_table[i] = weap;
	weap = weap->next;
    }

    return num_weapon_types;

}


weap_type_t *weapon_type(byte type)
{
    if( type >= num_weapon_types ) {
	printf("Error!, no weapon of type %u\n", (unsigned int)type);
	return NULL;
    }

    return weapon_type_table[ type ];

}


/* Returns weapon type for name "name" -1 on error */
int match_weapon_type(char *name)
{
    weap_type_t *wt;
    int i;

    if( !strcasecmp("NONE", name) )
	return 0;

    for( i=0 ; i < num_weapon_types ; i++) {
	if( (wt = weapon_type(i)) == NULL )
	    break;
	if( strcasecmp(wt->name, name) == 0 )
	    return i;
    }

    printf("NO MATCH for weapon type of name \"%s\"!\n", name);
    return -1;

}


ammo_t match_ammo_type(char *ammo)
{
    int i;

    for( i=0 ; i < NUM_AMMO_TYPES ; i++ ) {
	if( !strcasecmp(ammo_name[i], ammo) )  /* ammo_name[] from ammo.h */
	    return i;
    }

    printf("Unknown ammo type \"%s\"\n", ammo);
    return 0; /* NONE, Default */

}


/* Returns a pointer to newly created weap_type_t, returns NULL on error */
static weap_type_t *weapon_type_alloc(void)
{
    weap_type_t *weap;
    
    if( (weap = (weap_type_t *)malloc(sizeof(weap_type_t))) == NULL)
	perror("Malloc");
    
    return weap;

}


static weap_type_t *create_weapon_type(void)
{
    weap_type_t *weap;

    /* Create weap_type_t */
    if( (weap = weapon_type_alloc()) == NULL ) {
	printf("Error allocating new weapon type in create_weapon_type()!\n");
	return NULL;
    }

    /* Defaults for weapon_type */
    memset(weap, 0, sizeof(*weap));
    weap->ammo_usage = 1;
    weap->number = 1;
    weap->entstop = 1;
    weap->wallstop = 1;
    weap->draw_particles = 1;

    /* Add it to the list */
    if( wt_root == NULL )
	wt_root = weap;

    if( wt_tail != NULL )
	wt_tail->next = weap;

    wt_tail = weap;
    weap->next = NULL; /* Sentinel at end of list. */

    return weap;

}


static void weap_type_handle_line(char *line, weap_type_t *weap)
{
    ent_type_t *et;
    char str[32], str1[32];
    int v1, i, error;
    int num_entity_types;

    num_entity_types = entity_types();
    error = 0;

    if( !strncmp(line, "obituary ", 9) ) {
	line += 9;
	
	if( str_format_count(line, 's') == 2 ) {
	    if( (weap->obituary = (char *)strdup(line)) == NULL ) {
		perror("malloc");
		error = 1;
	    }
	} else {
	    printf("obituary \"%s\" not valid!\n", line);
	    error = 1;
	}
    } else if( sscanf(line, "%s %d", str, &v1) == 2 ) {
	if( !strcasecmp("damage", str) )
	    weap->damage = v1;
	else if( !strcasecmp("speed", str) )
	    weap->speed = v1;
	else if( !strcasecmp("reload_time", str) )
	    weap->reload_time = v1;
	else if( !strcasecmp("number", str) )
	    weap->number = v1;
	else if( !strcasecmp("spread", str) )
	    weap->spread = v1;
	else if( !strcasecmp("max_length", str) )
	    weap->max_length = v1;
	else if( !strcasecmp("gun_frames", str) )
	    weap->frames = v1;
	else if( !strcasecmp("entstop", str) )
	    weap->entstop = v1;
	else if( !strcasecmp("wallstop", str) )
	    weap->wallstop = v1;
	else if( !strcasecmp("mflash", str) )
	    weap->muzzle_flash = v1;
	else if( !strcasecmp("draw_particles", str) )
	    weap->draw_particles = v1;
	else if( !strcasecmp("explosion", str) )
	    weap->explosion = v1;
	else if( !strcasecmp("splashdamage", str) )
	    weap->splashdamage = v1;
	else if( !strcasecmp("initial_ammo", str) )
	    weap->initial_ammo = v1;
	else if( !strcasecmp("ammo_usage", str) )
	    weap->ammo_usage = v1;
	else if( !strcasecmp("push_factor", str) )
	    weap->push_factor = v1;
	else
	    error = 1;
    } else if( sscanf(line, "%s %s", str, str1) == 2 ) {
	if( !strcasecmp("class", str) ) {
	    if( !strcasecmp("PROJECTILE", str1) )
		weap->class = WT_PROJECTILE;
	    else if( !strcasecmp("BULLET", str1) )
		weap->class = WT_BULLET;
	    else if( !strcasecmp("SLUG", str1) )
		weap->class = WT_SLUG;
	    else if( !strcasecmp("BEAM", str1) )
		weap->class = WT_BEAM;
	    else
		error = 1;
	} else if( !strcasecmp("projectile", str) ) {
	    for( i=0 ; i < num_entity_types ; i++ ) {
		if( (et = entity_type(i)) == NULL ) {
		    error = 1;
		    break;
		}
		if( !strcasecmp(et->name, str1) )
		    weap->projectile = i;
	    }
	} else if( !strcasecmp("ammo", str) ) {
	    weap->ammo_type = match_ammo_type(str1);
	} else if( !strcasecmp("gun_image", str) ) {
	    strncpy(weap->weapon_image, str1, 32);
	    weap->has_weapon_image = 1;
	} else if( !strcasecmp("icon", str) ) {
	    strncpy(weap->weapon_icon, str1, 32);
	    weap->has_weapon_icon = 1;
	} else
	    error = 1;
    } else
	error = 1;

    if( error )
	printf("Error parsing line: \"%s\"\n", line);

}
