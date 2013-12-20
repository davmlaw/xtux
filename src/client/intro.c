#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "entity.h"
#include "input.h"
#include "draw.h"
#include "cl_net.h"

#define MAXLINE 1024
#define BIOFILE "bios"
#define BIO_LINE_LEN 80

extern int num_entity_types;
extern win_t win;
extern client_t client;

static char *get_character_info(char *name, int *infolines, char *filename)
{
    FILE *file;
    char line[MAXLINE];
    char str[8];
    char *bio;
    int found;
    int i, name_len;

    if( !(file = open_data_file(NULL, filename)) ) {
	return NULL;
    }

    bio = NULL;
    found = 0;
    name_len = strlen(name);

    while( !feof( file ) ) {
	if( !fgets( line, MAXLINE, file ) )
	    continue;
	if( line[0] == '#' )
	    continue;

	CHOMP(line);

	if( !found ) {
	    if( line[0] == '[' ) {
		/* set i to length of name between the brakets */
		for( i=1 ; line[i+1] != ']' && i < 31 ; i++ )
		    ;
		if( !strncasecmp(name, line + 1, name_len ) )
		    found = 1;
	    }
	} else { /* Found */

	    if( sscanf(line, "%s %d", str, infolines) != 2 ) {
		printf("Error parsing entity biography file\n");
		return NULL;
	    }
	    if( strcasecmp( "info_lines", str ) ) {
		printf("Error parsing entity biography file\n");
		return NULL;
	    }
	    if( client.debug )
		printf("%s has %d infolines in bio\n", name, *infolines);

	    if( (bio = (char *)malloc(BIO_LINE_LEN * *infolines)) == NULL ) {
		perror("Malloc");
		ERR_QUIT("Malloc Error!", -1);
	    }
	    memset( bio, 0, BIO_LINE_LEN * *infolines );

	    for( i=0 ; i < *infolines ; i++ ) {
		if( !fgets( line, MAXLINE, file ) )
		    continue;
		CHOMP(line);
		strncpy( &bio[i*BIO_LINE_LEN], line, BIO_LINE_LEN );
	    }		
	    break; /* Got the bio line(s) */
	}

    }

    fclose(file);
    return bio;

}


static void intro_entities(void)
{
    netmsg_entity ent;
    ent_type_t *edt;
    int infolines;
    int action;
    int type;
    int key;
    int i;
    short x;
    char *str;
    clear_t clear_type;

    clear_type = rand()%3;
    /* Return if key was pressed while cool_clear was happening */
    if( cool_clear( client.view_w, client.view_h, 2, clear_type ) )
	return;

    i = client.view_h/2 - 72;
    win_center_print(win.buf, "XTux", i, 2, "white"); i += 32;
    win_center_print(win.buf, "a game by", i, 2, "white"); i += 32;
    win_center_print(win.buf, "David Lawrence", i, 2, "white"); i += 32;
    win_center_print(win.buf, "&", i, 2, "white"); i += 32;
    win_center_print(win.buf, "James Andrews", i, 2, "white"); i += 32;
    win_update();
    delay( M_SEC * 3 );

    ent.type = NETMSG_ENTITY;
    ent.mode = ALIVE;
    ent.img = 0;
    ent.weapon = 0;
    ent.dir = 128;
    ent.y = client.view_h / 2;

    for( type = 0 ; type < num_entity_types ; type++ ) {
	ent.entity_type = type;
	if( (edt = entity_type( type )) == NULL ) {
	    printf("Could not get data for entity type %d\n", type);
	    return;
	}

	if((str = get_character_info(edt->name, &infolines, "bios")) == NULL)
	    continue;

	for( x = 0, action = 0 ; x < client.view_w ; x += 2 ) {
	    /*
	      action:
	      -1    Don't do any actions ever again
	      0    Nothing
	      1    Turning
	      ... more to come? running? Firing weapon?
	    */
	    if( action >= 0 && x > client.view_w / 3 ) {
		action = 1;
		ent.dir += 32;
		if( ent.dir == 128 )
		    action = -1;
	    }	    

	    clear_area(win.buf, 0, 0, client.view_w, client.view_h, "black");
	    ent.x = x;
	    entity_draw(ent);

	    for( i=0 ; i < infolines ; i++ ) {
		win_print(win.buf, &str[i*BIO_LINE_LEN],
			  0, client.view_h-x+i*font_height(1), 1, "white");
	    }

	    win_update();
	    delay(M_SEC / 12);

	    /* User input. Keypress takes us back to the menu */
	    key = get_key();
	    if( key == XK_Right )
		break;
	    else if( key != XK_VoidSymbol )
		return;

	}

	free( str );

    }

}

#define NUM_DEMOS 4
void do_intro(void)
{
    int n;
    char *demo[NUM_DEMOS] = {
	"demos/run.xtdem",
	"demos/slash.xtdem",
	"demos/bug.xtdem",
	"demos/quigley.xtdem"
    };

    n = rand()%(NUM_DEMOS+1);

    if( n < NUM_DEMOS ) {
	client.demo = DEMO_PLAY;
	client.demoname = data_file( demo[n] );
	cl_network_connect(NULL, 42);
	client.state = GAME_LOAD;
    } else
	intro_entities();

}
