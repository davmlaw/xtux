#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "sv_net.h"
#include "sv_netmsg_send.h"
#include "sv_netmsg_recv.h"
#include "sv_map.h"
#include "event.h"
#include "game.h"

#include "xtuxggz.h"

extern server_t server;
extern game_t game;
extern int num_entities;

/*
  FIXME: We really should have a timeout/keepalive system in place for
  clients, and no-ops should be there to say that the clients still active
*/
int sv_netmsg_recv_noop(client_t *cl, netmsg msg)
{
    printf("Got no-op from %s\n", cl->name);
    return 0;
}

int sv_netmsg_recv_query_version(client_t *cl, netmsg msg)
{
    sv_netmsg_send_version(cl);
    return 0;
}


int sv_netmsg_recv_version(client_t *cl, netmsg msg)
{
    printf("Remote version for %s = %c.%c.%c\n", cl->name,
	   msg.version.ver[0], msg.version.ver[1], msg.version.ver[2]);
    return 0;
}


int sv_netmsg_recv_textmessage(client_t *cl, netmsg msg)
{

    strncpy(msg.textmessage.sender, cl->name, 32);
    printf("%s:%s\n", msg.textmessage.sender, msg.textmessage.string);
    sv_net_send_to_all(msg);
    return 0;

}


int sv_netmsg_recv_quit(client_t *cl, netmsg msg)
{
    sv_net_remove_client(cl);
    return 0;
}


int sv_netmsg_recv_join(client_t *cl, netmsg msg)
{
    int change = 0; /* Should we change the current map/game? */
    char buf[NETMSG_STRLEN];

    server.clients++;
    cl->status = JOINING;
    if( !server.with_ggz )
    	strncpy(cl->name, msg.join.name, NETMSG_STRLEN);
    else
	xtuxggz_find_ggzname(cl->nc->fd, cl->name, NETMSG_STRLEN);
    printf("%s attempting to JOIN (%d clients)\n", cl->name, server.clients);

    if( msg.join.protocol != NETMSG_PROTOCOL_VERSION ) {
	printf("Client attempted join with wrong protocol\n");
	snprintf(buf, NETMSG_STRLEN, "Wrong protocol %d (serv = %d)",
		 msg.join.protocol, NETMSG_PROTOCOL_VERSION);
	sv_netmsg_send_rejection(cl->nc, buf);
	sv_net_remove_client(cl);
	return -1;
    }

    if( !strcasecmp(msg.join.name, "SERVER") ) {
	printf("Joining client name eq SERVER!\n");
	sv_netmsg_send_rejection(cl->nc, "Can't have name eq Server");
	sv_net_remove_client(cl);
	return -1; /* Client is now removed */
    }

    /* Allow changing of server details if newly joining client is
       the only one on the server */
    if( server.clients == 1 ) {
	if( strcmp(msg.join.map_name, game.map_name) )
	    change = 1;	/* Different map */
	else if( msg.join.gamemode != HOLYWAR || game.mode != HOLYWAR )
	    change = 1; /* Only stay if current AND requested mode = holywar */
    }

    if( change )
	if( sv_map_changelevel(msg.join.map_name, msg.join.gamemode) < 0 ) {
	    ERR_QUIT("Error changing levels", EXIT_FAILURE);
	}

    return 0;

}


/* The first "ready" enters the client to the game, any further
   ones only change the client details */
int sv_netmsg_recv_ready(client_t *cl, netmsg msg)
{
    char buf[TEXTMESSAGE_STRLEN];

    printf("GOT READY\n");

    if( cl->status < JOINING ) {
	printf("ERROR! Client has not yet sent join!\n");
	sv_netmsg_send_rejection(cl->nc, "No \"join\" sent");
	sv_net_remove_client(cl);
	return -1;
    }

    if( cl->status == JOINING ) {
	if( cl->ent == NULL ) {
	    cl->ent = entity_new( msg.ready.entity_type, 0, 0, 0, 0);
	    cl->ent->controller = CTRL_CLIENT;
	    cl->ent->name = cl->name;
	    cl->ent->cl = cl;
	}
	spawn(cl->ent, 0);
	snprintf(buf, TEXTMESSAGE_STRLEN, "%s (%s) entered the game",
		 cl->name, cl->ent->type_info->name);
	sv_net_send_text_to_all(buf);
    }

    cl->status = ACTIVE;
    cl->ent->color1 = msg.ready.color1;
    cl->ent->color2 = msg.ready.color2;
    cl->movemode = msg.ready.movemode;
    cl->view_w = msg.ready.view_w;
    cl->view_h = msg.ready.view_h;
    cl->health = cl->ent->health;
    cl->weapon = cl->ent->weapon;
    sv_netmsg_send_update_statusbar(cl);
    return 0;

}


int sv_netmsg_recv_query_sv_info(client_t *cl, netmsg msg)
{
    sv_netmsg_send_sv_info(cl);
    return 0;
}


int sv_netmsg_recv_cl_update(client_t *cl, netmsg msg)
{
    if( cl->status == ACTIVE ) {
	if( cl->ent->mode >= ALIVE )
	    cl->ent->dir = msg.cl_update.dir;
	cl->keypress = msg.cl_update.keypress;
    }

    return 0;

}


int sv_netmsg_recv_killme(client_t *cl, netmsg msg)
{
    point useless;

    if( cl->ent->health > 0 )
	entity_killed(cl->ent, cl->ent, useless, 0);

    return 0;

}


int sv_netmsg_recv_query_frags(client_t *cl, netmsg msg)
{
    sv_netmsg_send_frags(cl, 0);
    return 0;
}


int sv_netmsg_recv_away(client_t *cl, netmsg msg)
{

    printf("%s sent away message\n", cl->name);

    if( cl->status == ACTIVE )
	cl->status = AWAY;
    return 0;
    
}
