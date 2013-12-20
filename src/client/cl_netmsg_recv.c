#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xtux.h"
#include "client.h"
#include "draw.h"
#include "entity.h"
#include "cl_net.h"
#include "cl_netmsg_recv.h"
#include "cl_netmsg_send.h"
#include "particle.h"
#include "text_buffer.h"

extern client_t client;
extern map_t *map;
extern netmsg_entity my_entity;

void cl_netmsg_recv_query_version(netmsg msg)
{
    cl_netmsg_send_version();
}


void cl_netmsg_recv_version(netmsg msg)
{
    printf("Remote version = %c.%c.%c\n",
	   msg.version.ver[0], msg.version.ver[1], msg.version.ver[2]);
}


void cl_netmsg_recv_textmessage(netmsg msg)
{
    char tmp[NETMSG_STRLEN + TEXTMESSAGE_STRLEN];

    if( !strcmp(msg.textmessage.sender, "SERVER") )
	text_buf_push(msg.textmessage.string, COL_WHITE, ALIGN_LEFT);
    else {
	snprintf(tmp, NETMSG_STRLEN + TEXTMESSAGE_STRLEN, "%s: %s",
		 msg.textmessage.sender, msg.textmessage.string);
	text_buf_push(tmp, COL_GREEN, ALIGN_LEFT);
    }

}


void cl_netmsg_recv_quit(netmsg msg)
{
    cl_net_connection_lost("Server sent QUIT");
}


/* We should only get this at login and if our network has been behaving
   badly. */
void cl_netmsg_recv_rejection(netmsg msg)
{
    char buf[64];

    snprintf(buf, 64, "Rejected: %s\n", msg.rejection.reason);
    cl_net_connection_lost(buf);

}


void cl_netmsg_recv_sv_info(netmsg msg)
{
    
    printf("Server name = %s. Map = %s, Mode = %d, Players = %d\n",
	   msg.sv_info.sv_name, msg.sv_info.map_name, msg.sv_info.game_type,
	   msg.sv_info.players);
    strncpy(client.map, msg.sv_info.map_name, NETMSG_STRLEN);
    client.gamemode = msg.sv_info.game_type;
    if( client.gamemode >= NUM_GAME_MODES )
	client.gamemode = 0;

}



/* If we recieve this, we have status of JOINING on the server till we are
   done changing level, where we send the ready message to rejoin the game */
void cl_netmsg_recv_changelevel(netmsg msg)
{

    printf("CHANGING LEVEL to \"%s\"\n", msg.changelevel.map_name);
    fflush(NULL);

    particle_clear();
    strncpy(client.map, msg.changelevel.map_name, NETMSG_STRLEN);
    map_close(&map);

    cl_change_map(client.map, client.gamemode);

}

 /* msecs since last update, used for animation purposes. */
static msec_t last_update;

void cl_netmsg_recv_start_frame(netmsg msg)
{
    if( client.state == GAME_JOIN ) {
	last_update = gettime();
	client.state = GAME_PLAY; /* Got first start frame, we're playing */
    }

    client.screenpos = msg.start_frame.screenpos;

    if( client.state == GAME_PLAY ) {
	/* Draw layers of game world which are below the entities */
	draw_map( client.screenpos );
	particles_draw( PTL_BOTTOM ); /* Bottom layer of particles */
    }

}


void cl_netmsg_recv_end_frame(netmsg msg)
{
    msec_t now;
    float secs;

    if( client.state == GAME_PLAY ) {
	entity_draw_list();
	particles_draw( PTL_TOP ); /* Top layer of particles */
	if( map->toplevel )
	    draw_map_toplevel( client.screenpos );

	now = gettime();
	secs = now - last_update;
	secs /= (float)M_SEC; /* Convert to seconds */
	particle_update( secs );
	last_update = now;
    }

}


void cl_netmsg_recv_entity(netmsg msg)
{
    if( client.state == GAME_PLAY )
	entity_add(msg.entity);
}


void cl_netmsg_recv_myentity(netmsg msg)
{
    my_entity = msg.entity;
    if( client.state == GAME_PLAY )
	entity_add(msg.entity);
}


void cl_netmsg_recv_particles(netmsg msg)
{
    create_particles(msg.particles);
}


void cl_netmsg_recv_update_statusbar(netmsg msg)
{

    if( client.ammo != msg.update_statusbar.ammo
	|| client.weapon != msg.update_statusbar.weapon
	|| client.frags != msg.update_statusbar.frags
	|| client.health != msg.update_statusbar.health ) {
	/* Update local copies */
	client.ammo = msg.update_statusbar.ammo;
	client.weapon = msg.update_statusbar.weapon;
	client.frags = msg.update_statusbar.frags;
	client.health = msg.update_statusbar.health;
	if( client.textentry == 0 )
	    draw_status_bar();
    }

}


void cl_netmsg_recv_gamemessage(netmsg msg)
{

    text_buf_push(msg.gamemessage.string, COL_RED, ALIGN_CENTER);

}


void cl_netmsg_recv_map_target(netmsg msg)
{

    client.map_target = msg.map_target.target;
    client.map_target_active = msg.map_target.active;
}


void cl_netmsg_recv_objective(netmsg msg)
{

    strncpy(client.objective, msg.objective.string, TEXTMESSAGE_STRLEN);
    printf("New game objective = %s\n", client.objective);

}


/* FIXME: Frags should be different from normal messages, ie they should
   have more than 4 lines, should go all at once, etc etc */
void cl_netmsg_recv_frags(netmsg msg)
{
    char buf[NETMSG_STRLEN + 6];

    snprintf(buf, NETMSG_STRLEN + 6,
	     "%4d:%32s", msg.frags.frags, msg.frags.name); 
    text_buf_push(buf, COL_WHITE, ALIGN_LEFT);
    printf("%s\n", buf);

}
