#include <stdio.h>
#include <string.h>

#include "xtux.h"
#include "client.h"
#include "cl_netmsg_send.h"

extern netconnection_t *server;
extern client_t client;

void cl_netmsg_send_noop(void)
{
    netmsg msg;

    msg.type = NETMSG_NOOP;
    net_put_message(server, msg);

}


void cl_netmsg_send_query_version(void)
{
    netmsg msg;

    msg.type = NETMSG_QUERY_VERSION;
    net_put_message(server, msg);    

}


void cl_netmsg_send_version(void)
{
    netmsg msg;
    
    msg.type = NETMSG_VERSION;
    msg.version.ver[0] = VER0;
    msg.version.ver[1] = VER1;
    msg.version.ver[2] = VER2;

    net_put_message(server, msg);

}


void cl_netmsg_send_textmessage(char *str)
{
    netmsg msg;

    msg.type = NETMSG_TEXTMESSAGE;
    strncpy(msg.textmessage.string, str, TEXTMESSAGE_STRLEN);
    net_put_message(server, msg);

}


void cl_netmsg_send_quit(void)
{
    netmsg msg;

    msg.type = NETMSG_QUIT;
    net_put_message(server, msg);

}


void cl_netmsg_send_join(void)
{
    netmsg msg;

    msg.type = NETMSG_JOIN;
    msg.join.gamemode = client.gamemode;
    msg.join.protocol = NETMSG_PROTOCOL_VERSION;
    strncpy(msg.join.name, client.player_name, NETMSG_STRLEN);
    strncpy(msg.join.map_name, client.map, NETMSG_STRLEN);
    net_put_message(server, msg);

}



void cl_netmsg_send_ready(void)
{
    netmsg msg;

    msg.type = NETMSG_READY;
    msg.ready.entity_type = client.entity_type;
    msg.ready.color1 = client.color1;
    msg.ready.color2 = client.color2;
    msg.ready.movemode = client.movement_mode;
    msg.ready.view_w = client.view_w;
    msg.ready.view_h = client.view_h;
    net_put_message(server, msg);

}


void cl_netmsg_send_query_sv_info(void)
{
    netmsg msg;

    msg.type = NETMSG_QUERY_SV_INFO;
    net_put_message(server, msg);

}


extern netmsg_entity my_entity;

void cl_netmsg_send_cl_update(byte keypress, byte dir)
{
    static byte last_keypress = 0;
    netmsg msg;

    /* Don't send if keypress or direction hasn't changed */
    if( keypress != last_keypress || dir != my_entity.dir ) {
	last_keypress = keypress;
	msg.type = NETMSG_CL_UPDATE;
	msg.cl_update.keypress = keypress;
	msg.cl_update.dir = dir;
	net_put_message(server, msg);
    }

}


void cl_netmsg_send_killme(void)
{
    netmsg msg;

    msg.type = NETMSG_KILLME;
    net_put_message(server, msg);

}


void cl_netmsg_send_query_frags(void)
{
    netmsg msg;

    msg.type = NETMSG_QUERY_FRAGS;
    net_put_message(server, msg);

}


void cl_netmsg_send_away(void)
{
    netmsg msg;

    msg.type = NETMSG_AWAY;
    net_put_message(server, msg);

}
