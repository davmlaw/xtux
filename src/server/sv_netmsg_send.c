#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "sv_net.h"
#include "sv_netmsg_send.h"
#include "game.h"

extern server_t server;
extern game_t game;

/* Send message MESSAGE_TYPE to client (Useful when message type not
   data within is important) */
void sv_netmsg_send_message(client_t *cl, byte message_type)
{
    netmsg msg;

    msg.type = message_type;
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_rejection(netconnection_t *nc, char *reason)
{
    netmsg msg;
  
    msg.type = NETMSG_REJECTION;
    strncpy( msg.rejection.reason, reason, 32 );
    net_put_message(nc, msg);
    printf("Rejected login from \"%s\", %s\n", nc->remote_address, reason);

}


void sv_netmsg_send_version(client_t *cl)
{
    netmsg msg;

    msg.type = NETMSG_VERSION;
    msg.version.ver[0] = VER0;
    msg.version.ver[1] = VER1;
    msg.version.ver[2] = VER2;
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_textmessage(client_t *cl, char *sender, char *string)
{
    netmsg msg;

    msg.type = NETMSG_TEXTMESSAGE;
    snprintf(msg.textmessage.sender, 32, sender);
    snprintf(msg.textmessage.string, 32, string);
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_sv_info(client_t *cl)
{
    netmsg msg;
    
    msg.type = NETMSG_SV_INFO;
    strncpy(msg.sv_info.map_name, game.map_name, 32);
    strncpy(msg.sv_info.sv_name, server.name, 32 );
    msg.sv_info.players = server.clients;
    msg.sv_info.game_type = game.mode;
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_changelevel(client_t *cl)
{
    netmsg msg;
    
    msg.type = NETMSG_CHANGELEVEL;
    strncpy(msg.changelevel.map_name, game.map_name, 32);
    printf("Changing level to \"%s\"\n",game.map_name); 
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_update_statusbar(client_t *cl)
{
    netmsg msg;

    msg.type = NETMSG_UPDATE_STATUSBAR;
    msg.update_statusbar.health = cl->health;
    msg.update_statusbar.ammo = cl->ammo;
    msg.update_statusbar.weapon = cl->weapon;
    msg.update_statusbar.frags = cl->frags;
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_gamemessage(client_t *cl, char *str, int everyone)
{
    netmsg msg;

    msg.type = NETMSG_GAMEMESSAGE;
    strncpy(msg.gamemessage.string, str, TEXTMESSAGE_STRLEN);
    if( everyone )
	sv_net_send_to_all(msg);
    else
	net_put_message(cl->nc, msg);

}


void sv_netmsg_send_map_target(client_t *cl, int active, shortpoint_t target)
{
    netmsg msg;

    msg.type = NETMSG_MAP_TARGET;
    msg.map_target.active = active;
    msg.map_target.target = target;
    net_put_message(cl->nc, msg);

}


void sv_netmsg_send_objective(client_t *cl, char *str, int everyone)
{
    netmsg msg;

    msg.type = NETMSG_OBJECTIVE;
    strncpy(msg.objective.string, str, TEXTMESSAGE_STRLEN);
    if( everyone )
	sv_net_send_to_all(msg);
    else
	net_put_message(cl->nc, msg);

}


/* We want to sort in descending order, so we return < 0 if a is bigger
   than b */
static int frag_cmp(const client_t **a, const client_t **b)
{

    if( (*a)->frags > (*b)->frags )
	return -1;
    else if( (*a)->frags < (*b)->frags )
	return 1;
    else
	return 0;
}


extern client_t *cl_root;

#define FRAGLISTSIZE 8

void sv_netmsg_send_frags(client_t *cl, int everyone)
{
    client_t *top[FRAGLISTSIZE];
    netmsg msg;
    int i, j;
    client_t *ptr, *lowest;

    /* printf("sending frags.... to %s\n", everyone? "everyone" : cl->name); */
    memset( top, 0, sizeof(client_t *) * FRAGLISTSIZE );
    i = 0;
    lowest = cl_root;

    /* Build a list of the top players */
    for( ptr = cl_root ; ptr != NULL ; ptr = ptr->next ) {
	if( i < FRAGLISTSIZE ) {
	    top[i] = ptr;
	    i++;
	} else { /* No room left, but perhaps replace current lowest value */
	    lowest = top[0];
	    for( j=1 ; j<FRAGLISTSIZE ; j++ ) {
		if( top[j]->frags < lowest->frags )
		    lowest = top[j];
	    }
	    if( ptr->frags > lowest->frags )
		lowest = ptr;
	}
    }

    /* sort in descending order according to frags */
    qsort(top,i,sizeof(client_t*),(int (*)(const void*,const void*))frag_cmp);

    /* Send them out to the client, in order */
    msg.type = NETMSG_FRAGS;
    for( i=0 ; i < FRAGLISTSIZE ; i++ ) {
	if( top[i] ) {
	    strncpy( msg.frags.name, top[i]->name, NETMSG_STRLEN );
	    msg.frags.frags = top[i]->frags;
	    if( everyone )
		sv_net_send_to_all(msg);
	    else
		net_put_message(cl->nc, msg);
	}
    }

}
