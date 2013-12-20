#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#include "xtux.h"
#include "client.h"
#include "input.h"
#include "win.h"

#define CONFIGFILE ".xtux"

extern int key_config[NUM_KEYS];
extern int mouse_config[NUM_MOUSE_BUTTONS];

static void parse_line(char *line, client_t *cl);
static int handle_str_int(client_t *cl, char *str, int num);
static int handle_str_str(client_t *cl, char *str, char *str1);


/* Calculate the width & heights of the text strings to
   later test to see if the text will appear on screen */ 
void maptext_init(maptext_t *root)
{
    maptext_t *ptr;

    ptr = root;
    while( ptr ) {
	ptr->width = text_width(ptr->string, ptr->font);
	ptr->height = font_height(ptr->font);
	ptr = ptr->next;
    }


}


/* Try to get the login name from environment variables, else use
   the password database entry, or "Anon-coward if that too fails */
char *get_login_name()
{
  struct passwd *pwd;
  char *username;

  if( (username = getenv("USER")) == NULL ) {
      if ((pwd = getpwuid(getuid())) == NULL)
	  return "Anon-Coward";
      else
	  return pwd->pw_name;
  } else
      return username;

}

/******************************************
 Config file misc reading/writing functions
*******************************************/

/* Buffer to store the absolute path of files, so we don't need to chdir() */
static char filename[PATH_MAX];
static char *toggle[] = { "off", "on" };
static char *nstats[] = { "none", "total", "recent" };

static FILE *open_config_file(char *mode)
{
    FILE *file;

    snprintf(filename, PATH_MAX, "%s/%s", get_home_dir(), CONFIGFILE);

    if( !(file = fopen( filename, mode )) ) {
	return NULL;
    }

    return file;

}


void read_config_file(client_t *cl)
{
    FILE *file;
    char line[MAXLINE];

    if( (file = open_config_file("r")) == NULL )
	return;

    while( !feof(file) ) {
	if( fgets( line, MAXLINE, file ) == NULL )
	    break;
	CHOMP(line);
	if( line[0] == '#' || line[0] == '\0' )
	    continue;
	parse_line(line, cl);
    }

    fclose(file);

}

static char *keyname[NUM_KEYS] = {
    "left",
    "right",
    "forward",
    "back",
    "s_left",
    "s_right",
    "b1",
    "b2",
    "weap_up",
    "weap_dn",
    "sniper",
    "screenshot",
    "ns_toggle",
    "rad_dec",
    "rad_inc",
    "textmode",
    "suicide",
    "show_frags"
};


void write_config_file(client_t *cl)
{
    FILE *file;
    ent_type_t *et;
    int i;

    if( (file = open_config_file("w")) == NULL ) {
	printf("Couldn't save config file!\n");
	return;
    }

    fprintf(file, "#XTux config file\n");
    fprintf(file, "name %s\n", cl->player_name);
    if( (et = entity_type(cl->entity_type)) != NULL )
	fprintf(file, "character %s\n", et->name);
    fprintf(file, "movemode %s\n", cl->movement_mode? "CLASSIC" : "NORMAL");
    fprintf(file, "mousemode %d\n", cl->mousemode);
    fprintf(file, "netstats %s\n", nstats[ cl->netstats ]);
    fprintf(file, "debug %s\n", toggle[ cl->debug ]);
    
    fprintf(file, "color1 %d\n", cl->color1);
    fprintf(file, "color2 %d\n", cl->color2);
    fprintf(file, "crossrad %d\n", cl->crosshair_radius);
    fprintf(file, "turnrate %d\n", cl->turn_rate);
    fprintf(file, "#screen dimensions (%d,%d)\n", 
	    cl->x_tiles * TILE_W, cl->y_tiles * TILE_H);
    fprintf(file, "x_tiles %d\n", cl->x_tiles);
    fprintf(file, "y_tiles %d\n", cl->y_tiles);
    fprintf(file, "ep_expire %d\n", cl->ep_expire);

    fprintf(file, "#KEY CONFIG\n");

    for( i=0 ; i<NUM_KEYS ; i++ )
	fprintf(file, "%s %s\n", keyname[i], XKeysymToString( key_config[i] ));

    for( i=0 ; i<NUM_MOUSE_BUTTONS ; i++ )
	fprintf(file, "mbutton%d %d\n", i+1, mouse_config[i]);

    fclose(file);

}



static void parse_line(char *line, client_t *cl)
{
    char str[32];
    int num, len;
    int error = 0;

    if( sscanf(line, "%s %d", str, &num) == 2 )
	error = handle_str_int(cl, str, num);
    else if( sscanf(line, "%s", str) == 1 ) {
	len = strlen(str);
	if( line[len] == ' ' )
	    error = handle_str_str(cl, str, line + len + 1);
	else
	    error = 1;
    } else
	error = 1;

    if( error )
	printf("***** Error parsing line: \"%s\" *****\n", line);

}


static int handle_str_int(client_t *cl, char *str, int num)
{

    if( !strcasecmp("color1", str) )
	cl->color1 = num;
    else if( !strcasecmp("color2", str) )
	cl->color2 = num;
    else if( !strcasecmp("crossrad", str) )
	cl->crosshair_radius = num;
    else if( !strcasecmp("turnrate", str) )
	cl->turn_rate = num;
    else if( !strcasecmp("x_tiles", str) )
	cl->x_tiles = num;
    else if( !strcasecmp("y_tiles", str) )
	cl->y_tiles = num;
    else if( !strcasecmp("ep_expire", str) )
	cl->ep_expire = num;
    else if( !strcasecmp("mousemode", str) )
	cl->mousemode = num;
    else if( !strcasecmp("mbutton1", str) )
	mouse_config[0] = num;
    else if( !strcasecmp("mbutton2", str) )
	mouse_config[1] = num;
    else if( !strcasecmp("mbutton3", str) )
	mouse_config[2] = num;
    else
	return 1;

    return 0;

}


static int handle_str_str(client_t *cl, char *str, char *str1)
{
    int key;
    int tmp, i;

    if( !strcasecmp("name", str) ) {
	free(cl->player_name);
	cl->player_name = (char *)strdup( str1 );
	return 0;
    } else if( !strcasecmp("character", str) ) {
	if( (tmp = match_entity_type(str1)) >= 0 ) {
	    cl->entity_type = tmp;
	    return 0;
	} else {
	    fprintf(stderr, "Unknown entity \"%s\"\n", str1);
	    cl->entity_type = 0;
	    return 1;
	}
    } else if( !strcasecmp("movemode", str) ) {
	if( !strcasecmp("classic", str1) )
	    cl->movement_mode = CLASSIC;
	else
	    cl->movement_mode = NORMAL;
	return 0;
    } else if( !strcasecmp("netstats", str) ) {
	for( i=0 ; i<NUM_NETSTATS ; i++ ) {
	    if( !strcasecmp(nstats[i], str1) )
		cl->netstats = i;
	}
	return 0;
    } else if( !strcasecmp("debug", str) ) {
	if( !strcasecmp("on", str1) )
	    cl->debug = 1;
	else
	    cl->debug = 0;
	return 0;
    } else {
	for( i=0 ; i<NUM_KEYS ; i++ ) {
	    if( !strcasecmp(keyname[i], str) ) {
		if( (key = XStringToKeysym(str1)) == NoSymbol )
		    printf("ERROR reading keysym \"%s\"\n", str1);
		else {
		    key_config[i] = key;
		    return 0;
		}
	    }
	}

    }

    return 1;

}

