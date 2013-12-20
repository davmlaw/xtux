#include <stdio.h>
#include <string.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "sv_map.h"
#include "game.h"
#include "clients.h"
#include "sv_net.h"
#include "sv_netmsg_send.h"

extern int class_population[NUM_ENT_CLASSES];
extern server_t server;

game_t game;
int told;

void game_start(void)
{

    if( game.timelimit ) {
	game.endtime = server.now + M_SEC * game.timelimit;
	printf("Timelimit = %d seconds\n", game.timelimit);
    } else
	game.endtime = 0;

}



void game_update(void)
{

    if( game.changelevel || (game.endtime && server.now > game.endtime) ) {
	if( game.next_map_name[0] == '\0' ) { /* Empty */
	    printf("Restarting current level\n");
	    sv_map_changelevel(game.map_name, game.mode); /* Restart map */
	} else {
	    printf("Changing level to %s\n", game.next_map_name);
	    sv_map_changelevel(game.next_map_name, game.mode);
	}
	game.changelevel = 0;
    }

    /* SAVETHEWORLD objective now complete? */
    if( game.objective_complete == 0 ) {
	if( game.mode == SAVETHEWORLD ) {
	    if( game.objective == GAME_ELIMINATE_CLASS ) {
		if( class_population[ game.class ] == 0 ) {
		    sv_netmsg_send_gamemessage(0, game.success_msg, 1);
		    game.objective_complete = 1;
		}
	    }
	}
    } else { /* Objective complete */
	if( game.endtime < server.now ) {
	    printf("This level will end in 3 seconds...\n");
	    game.endtime = server.now + M_SEC * 3;
	}
    }


}


void game_close(void)
{

    memset(game.map_name, 0, NETMSG_STRLEN);
    memset(game.next_map_name, 0, NETMSG_STRLEN);
    strncpy(game.success_msg, "Sorted.", TEXTMESSAGE_STRLEN);
    game.changelevel = 0;
    game.objective = 0;
    game.objective_complete = 0;

}
