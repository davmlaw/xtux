#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "sv_net.h"
#include "world.h"
#include "sv_map.h"
#include "weapon.h"
#include "game.h"

#include "xtuxggz.h"

server_t server;
map_t *map = NULL;

/*  located in common/math */
extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];
extern game_t game;

void handle_cl_args(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    float tick;

    /* Server defaults */
    server.port = DEFAULT_PORT;
    server.fps = DEFAULT_FPS;
    server.max_clients = DEFAULT_MAXCLIENTS;
    server.clients = 0;
    server.exit_when_empty = 0;
    strncpy(server.name, "Chickens With Attitude", 32);

    /* Game defaults */
    game.changelevel = 0;
    game.fraglimit = 0;
    game.timelimit = 0;
    game.mode = HOLYWAR;
    game.objective = GAME_EXIT_LEVEL;
    strncpy(game.map_name, "unfinished_sympathy.map", NETMSG_STRLEN);

    /* Command line arguments */
    if( argc > 1 )
	handle_cl_args(argc, argv);

    calc_lookup_tables(DEGREES);
    entity_init();
    weapon_init();

    /* Initilaise networking. TRY to use server.port, but return the actual
       port we manage to get */
    if( (server.port = sv_net_init(server.port)) >= 0 )
	printf("Waiting for connections on port %d\n", server.port);
    else {
	printf("Unable to initialise network. Exiting.\n");
	return EXIT_FAILURE;
    }

    if( sv_map_changelevel(game.map_name, game.mode) < 0 ) {
	printf("Error, unable to load map\n");
	return EXIT_FAILURE;
    }

    tick = 1.0 / server.fps;
    server.quit = 0;

    /* Server main loop */
    while( !server.quit ) {
	/*
	  printf("Press to continue.....\n");
	  fflush(NULL);
	  getchar();
	*/

	server.now = gettime();
	sv_net_get_client_input();
	world_update( tick );
	game_update();
	sv_net_send_start_frames(); /* Begin each clients frames */
	sv_net_update_clients();
	cap_fps(server.fps);
    }

    return EXIT_SUCCESS;

}


void handle_cl_args(int argc, char *argv[])
{
  int i, f, exit_status;

  exit_status = EXIT_FAILURE;
  
  for(i = 1; i < argc ; i++) {
      if(argv[i][0] == '-') {
	  switch(argv[i][1]) {
	  case 'c':
	      if( i < argc - 1 )
		  server.max_clients = atoi( argv[++i] );
	      break;
	  case 't':
	      if( i < argc - 1 )
		  game.timelimit = atoi( argv[++i] );
	      break;
	  case 'k':
	      if( i < argc - 1 )
		  game.fraglimit = atoi( argv[++i] );
	      break;
	  case 'm':
	      if( i < argc - 1 ) { /* There are more args */
		  strncpy( game.map_name, argv[++i], 32);
		  printf("selected map name of %s\n", game.map_name);
	      }
	      break;
	  case 'p':
	      if( i < argc - 1 )
		  server.port = atoi( argv[++i] );
	      break;
	  case 'f':
	      if( i < argc - 1 ) {
		  f = atoi( argv[++i] );
		  if( f )
		      server.fps = f;
		  else
		      printf("error parsing FPS switch\n");
	      }
	      break;
	  case 'g': /* GGZ mode */
	      server.with_ggz = 1;
	      if( xtuxggz_init() ==-1)
		      exit(-1);
	      break;
	  case 'v': /* Print version number */
	      printf("%s\n", VERSION);
	      exit(EXIT_SUCCESS);
	  case 'e':
	      server.exit_when_empty = 1;
	      break;
	  case 'h': /* Help (default) */
	      exit_status = EXIT_SUCCESS;
	  default:
	      printf("usage: %s [OPTIONS]\n"
		     "  -c CLIENTS Set maxclients to CLIENTS\n"
		     "  -t SECONDS Set timelimit to SECONDS\n"
		     "  -k FRAGS   Set fraglimit to FRAGS\n"
		     "  -m MAP     select map of name MAP\n"
		     "  -p PORT    use port PORT\n"
		     "  -g         enables GGZ mode\n"
		     "  -v         Print version number.\n"
		     "  -e         Exit after the last client leaves.\n"
		     "  -h         Display help (this screen)\n\n"
		     "This product is FREE SOFTWARE and comes with "
		     "ABSOLUTELY NO WARRANTY!\n"
		     "Report bugs to philaw@ozemail.com.au\n", argv[0]);
	      exit(exit_status);
	  }
      }
  }

}
