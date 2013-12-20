#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xtux.h"
#include "server.h"
#include "entity.h"
#include "clients.h"
#include "misc.h"
#include "sv_net.h"
#include "sv_netmsg_recv.h"
#include "sv_netmsg_send.h"
#include "event.h"

#include "xtuxggz.h"

#ifndef INADDR_ANY
#define INADDR_ANY      (0x00000000) 
#endif

#define BACKLOG 2  /* Amount of pending connections we allow on sock */
/* Maximum times we will have read errors from a client before dropping them */
#define MAX_BAD 64

extern server_t server;
extern entity *root;
extern float sin_lookup[DEGREES];
extern float cos_lookup[DEGREES];

client_t *cl_root = NULL;
client_t *cl_tail = NULL;

/* Sock is used for listening for incoming connections */
static int sock;

static void sv_net_update(void);
static void sv_net_update_statusbar(client_t *cl);
static void sv_net_handle_client(client_t *cl);
static void sv_net_add_client(int fd);
static int sv_net_handle_message(client_t *cl, netmsg msg);
static void sv_net_add_client(int fd);
static int sv_net_handle_message(client_t *cl, netmsg msg);
static void sv_net_handle_client(client_t *cl);
static void sv_net_update_clients_entity(client_t *cl);

short sv_net_init(short port)
{
    struct sockaddr_in addr;
   
    if( server.with_ggz ) {
	sock = xtuxggz_giveme_sock();
    } else {
    	sock = net_init_socket();
	memset((char*)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;

	/* Edited 23 Jan 2003 now we only bind to desired port */

	addr.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	    printf("Couldn't bind to port %d.\n", port);
	    perror("bind");
	    return -1;
	}

    	/* listen for new connections, allow BACKLOG pending */
	if (listen(sock, BACKLOG) < 0) {
	    perror("listen");
	    return -1;
	}
    }

    /* Set server socket to nonblocking */
    fcntl(sock, F_SETFL, O_NONBLOCK);
    return port; /* Port we are bound to */

}


void sv_net_get_client_input(void)
{
    netconnection_t *nc;
    client_t *cl, *next;
    int rfd;

    sv_net_update();

    for( cl = cl_root ; cl != NULL ; cl = next ) {
	nc = cl->nc;
	next = cl->next;
	if( nc->incoming ) {
	    if( (rfd = net_input_flush( cl->nc, NM_ALL )) > 0 ) {
		sv_net_handle_client( cl );
		nc->incoming = 0;
	    } else if( rfd == 0 ) {
		if( ++cl->bad >= MAX_BAD ) {
		    sv_netmsg_send_rejection(cl->nc, "Too many read errors");
		    sv_net_remove_client(cl);
		    continue;
		} else
		    printf("Read error %d for %s\n", cl->bad, cl->name);
	    } else {
		printf("Input error from \"%s\" (%s).\n",
		       cl->name, cl->nc->remote_address);
		sv_net_remove_client(cl);
		continue;
	    }
	}
	if( cl->status == ACTIVE )
	    sv_net_update_clients_entity( cl );
    }

}


void sv_net_send_start_frames(void)
{
    client_t *cl;
    netmsg msg;

    for( cl = cl_root ; cl != NULL ; cl = cl->next ) {
	if( cl->status != ACTIVE )
	    continue; /* Skip inactive clients */
	cl->screenpos = calculate_screenpos(cl);
	msg.type = NETMSG_START_FRAME;
	msg.start_frame.screenpos = cl->screenpos;
	net_put_message(cl->nc, msg);
    }

}


static void sv_net_send_entities(client_t *cl)
{
    netmsg msg;
    entity *ent;
    animation_t *ani;

    /* Send each entitiy the client can see */
    for( ent = root ; ent != NULL ; ent = ent->next ) {
	if( ent->mode == LIMBO )
	    continue;

	/* Only the client can see their own invisible entity */
	if( ent->visible == 0 && cl->ent != ent )
	    continue;
	
	ani = entity_type_animation( ent->type_info, ent->mode );
	
	/* Skip if X is out of range */
	if( ent->x + ani->img_w < cl->screenpos.x )
	    continue;
	if( ent->x > cl->screenpos.x + cl->view_w )
	    continue;
	
	/* Skip if Y is out of range */
	if( ent->y + ani->img_h < cl->screenpos.y )
	    continue;
	if( ent->y > cl->screenpos.y + cl->view_h )
	    continue;
	
	msg = entity_to_netmsg(ent);
	if( cl->ent == ent ) {
	    msg.type = NETMSG_MYENTITY;
	} else
	    msg.type = NETMSG_ENTITY;
	net_put_message(cl->nc, msg);
    }
	    
}




void sv_net_update_clients(void)
{
    client_t *cl;

    for( cl = cl_root ; cl != NULL ; cl = cl->next ) {
	if( cl->status == ACTIVE ) {
	    sv_net_update_statusbar(cl);
	    event_draw(cl);
	    sv_net_send_entities(cl);
	    sv_netmsg_send_message(cl, NETMSG_END_FRAME);
	}

	net_output_flush(cl->nc);

    }


}


void sv_net_send_to_all(netmsg msg)
{
    client_t *cl;

    for( cl = cl_root ; cl != NULL ; cl = cl->next )
	if( cl->status >= JOINING ) {
	    net_put_message( cl->nc, msg);
	}

}


void sv_net_send_text_to_all(char *message)
{
    netmsg msg;

    printf("%s\n", message);

    msg.type = NETMSG_TEXTMESSAGE;
    strncpy( msg.textmessage.sender, "SERVER", NETMSG_STRLEN );
    strncpy( msg.textmessage.string, message, TEXTMESSAGE_STRLEN );
    sv_net_send_to_all(msg);

}


void sv_net_tell_clients_to_changelevel(void)
{
    client_t *cl;
    netmsg msg;

    for( cl = cl_root ; cl != NULL ; cl = cl->next ) {
	if( cl->status == ACTIVE || cl->status == AWAY ) {
	    cl->status = JOINING; /* Clients will send ready notices */
	    cl->ent->mode = LIMBO;
	    cl->ent->frags = 0;
	}

	sv_netmsg_send_changelevel(cl);
	msg.type = NETMSG_END_FRAME;
	net_put_message(cl->nc, msg);
    }

}


void sv_net_remove_client(client_t *cl)
{
    char buf[TEXTMESSAGE_STRLEN];

    net_close_connection(cl->nc);
    if( cl->status >= JOINING )
	server.clients--;
    snprintf(buf, TEXTMESSAGE_STRLEN, "%s disconnected (%d clients remain)",
	     cl->name, server.clients);
    sv_net_send_text_to_all(buf);

    if( server.clients <= 0 && server.exit_when_empty ) {
	printf("Closing Empty Server.\n");
	server.quit = 1;
    }

    if( cl->ent ) {
	entity_delete( cl->ent );
    }

    /* Make sure that root & Tail will still point at the
       right spot after cl is deleted */
    if( cl_root == cl )
	cl_root = cl->next;
    if( cl_tail == cl )
	cl_tail = cl->prev;

    if( cl->prev )
	cl->prev->next = cl->next;

    if( cl->next )
	cl->next->prev = cl->prev;

    free(cl);


}


static void sv_net_update(void)
{
    char buf[32];
    struct timeval tv;
    struct sockaddr_in naddr;
    client_t *cl;
    netconnection_t *nc;
    fd_set read_fds;
    int newsock;
    int maxfd; /* Highest fd + 1 needed for select() */
    int addrlen; /* addrlen is passed to accept() and is changed to be the
		    length of the new address */

    FD_ZERO(&read_fds);

    /* If status of sock changes, then we have a new client wishing to join
       the server */
    FD_SET(sock, &read_fds);
    maxfd = sock;

    /* The fd's change status when there is new client data to be read */
    for( cl = cl_root ; cl != NULL ; cl = cl->next ) {
	FD_SET(cl->nc->fd, &read_fds);
	if( cl->nc->fd > maxfd )
	    maxfd = cl->nc->fd;
    }
    
    tv.tv_sec = 0;
    tv.tv_usec = M_SEC / 20;
    select(maxfd + 1, &read_fds, 0, 0, &tv);

    if( FD_ISSET(sock, &read_fds) ) {
	if( server.with_ggz ) {
	    if( xtuxggz_is_sock_set(&newsock) != 1 )
		newsock=-1;
	    else
    		fprintf(stderr,"-----NEW PLAYER----\n");
	} else {
	    addrlen = sizeof(naddr);
	    newsock = accept(sock,(struct sockaddr *)&naddr,(socklen_t *)&addrlen);
	}

	if( newsock < 0 ) {
	    perror("accept");
	} else {
	    if( server.clients < server.max_clients ) {
		fcntl( newsock, F_SETFL, O_NONBLOCK); /* Set to non blocking */
		sv_net_add_client(newsock);
	    } else { /* Too many clients, send rejection */
		snprintf(buf, 32, "Server full (max=%d)", server.max_clients);
		if( (nc = net_new_connection(newsock)) ) {
		    sv_netmsg_send_rejection(nc, buf);
		    net_close_connection(nc);
		}
	    }
	}
    }

    /* Read data from each ACTIVE client if it's fd status has changed */
    for( cl = cl_root ; cl != NULL ; cl = cl->next )
	if( FD_ISSET(cl->nc->fd, &read_fds) )
	    cl->nc->incoming = 1;

}


#define CHECK_CHANGE(a)       \
          if( cl-> a != cl->ent-> a ) { cl-> a = cl->ent-> a ; change = 1; } 

/* Checks to see if client cl needs it's statusbar updated */
static void sv_net_update_statusbar(client_t *cl)
{
    weap_type_t *wt;
    int change = 0;

    CHECK_CHANGE(health);
    if( change && cl->ent->health < 0 )
	cl->health = 0; /* Health is sent as byte, so no negative values */

    CHECK_CHANGE(weapon);
    CHECK_CHANGE(frags);

    wt = weapon_type( cl->ent->weapon );
    if( cl->ammo != cl->ent->ammo[ wt->ammo_type ] ) {
	cl->ammo = cl->ent->ammo[ wt->ammo_type ];
	change = 1;
    }

    if( change )
	sv_netmsg_send_update_statusbar(cl);

}


static void sv_net_handle_client(client_t *cl)
{
    netmsg msg;

    while( (msg = net_next_message(cl->nc, NM_ALL)).type != NETMSG_NONE )
	if( sv_net_handle_message(cl, msg) == NETMSG_QUIT )
	    break; /* If we get a quit, the rest are irrelevant */

}


/* Change entities velocity in game according to clients keypresses */
static void sv_net_update_clients_entity(client_t *cl)
{
    entity *ent;
    int accel;
    byte dir;

    ent = cl->ent; /* Saves typing */
    if( ent->mode < ALIVE )
	return; /* ent is not allowed to move */

    accel = ent->type_info->accel;

    if( cl->movemode == NORMAL ) {
	if( cl->keypress & FORWARD_BIT ) {
	    ent->x_v += sin_lookup[ ent->dir ] * accel;
	    ent->y_v -= cos_lookup[ ent->dir ] * accel;
	}
	
	if( cl->keypress & BACK_BIT ) {
	    ent->x_v -= sin_lookup[ ent->dir ] * accel;
	    ent->y_v += cos_lookup[ ent->dir ] * accel;
	}
	
	if( cl->keypress & LEFT_BIT ) {
	    dir = ent->dir - DEGREES / 4; /* 1/4 rotation LEFT */
	    ent->x_v += sin_lookup[ dir ] * accel;
	    ent->y_v -= cos_lookup[ dir ] * accel;
	}
    
	if( cl->keypress & RIGHT_BIT ) {
	    dir = ent->dir + DEGREES / 4; /* 1/4 rotation RIGHT */
	    ent->x_v += sin_lookup[ dir ] * accel;
	    ent->y_v -= cos_lookup[ dir ] * accel;
	}
    } else { /* Classic movement mode */
	int movechange = 0;

	if( cl->keypress & FORWARD_BIT )
	    movechange = 1, ent->y_v = -accel;
	if( cl->keypress & BACK_BIT )
	    movechange = 1, ent->y_v = accel;
	if( cl->keypress & LEFT_BIT )
	    movechange = 1, ent->x_v = -accel;
	if( cl->keypress & RIGHT_BIT )
	    movechange = 1, ent->x_v = accel;

	if( movechange ) {
	    vector_t U, V;
	    U.x = 0;        /* unit vector pointing up (dir = 0) */
	    U.y = 1;
	    V.x = 1000 * ent->x_v; /* vector addition of ent velocity */
	    V.y = -1000 * ent->y_v;
	    ent->dir = angle_between(U, V);
	}
    }

    /* Have entity (attempt to) fire it's weapon */
    if( cl->keypress & B1_BIT ) {
	ent->trigger = 1;
    } else
	ent->trigger = 0;

    if( cl->keypress & WEAP_UP_BIT )
	ent->weapon_switch = 1;
    else if( cl->keypress & WEAP_DN_BIT )
	ent->weapon_switch = -1;
    else
	ent->weapon_switch = 0;

}


/* Add the client that's been accept()-ed onto fd */
static void sv_net_add_client(int fd)
{
    netconnection_t *nc;
    client_t *cl;

    if( (nc = net_new_connection(fd)) == NULL ) {
	printf("Error creating new net connection.\n");
	return;
    }

    if( (cl = (client_t *)malloc(sizeof(client_t))) == NULL ) {
	perror("malloc");
	sv_netmsg_send_rejection(nc, "server memory error");
	net_close_connection(nc);
	return;
    }

    /* Add client to the list */
    if( cl_root == NULL )
	cl_root = cl;
    if( cl_tail != NULL )
	cl_tail->next = cl;

    cl->prev = cl_tail;
    cl->next = NULL;
    cl_tail = cl;

    cl->nc = nc;
    strncpy(cl->name, "Connecting Client", NETMSG_STRLEN);
    cl->status = CONNECTING; /* Waiting for join message */
    cl->bad = 0;
    cl->ent = NULL;
    cl->keypress = 0;

    printf("Client joining from \"%s\"\n", nc->remote_address);

}


/*
  Function pointer table for handling incoming messages.
  All of the below functions are in "sv_netmsg_recv.c" and have
  the form "int function(client_t *cl, netmsg msg)" (arg1 = pointer to
  client_t, arg2 = netmsg, returning void).
  We discard the messages that are NOTHANDLED, because
  well behaved clients shouldn't ever send them.
*/


static int sv_netmsg_not_handled(client_t *cl, netmsg msg)
{

    printf("NOT HANDLING MESSAGE \"%s\" from \"%s\" (%s)\n",
	   net_message_name(msg.type), cl->name, cl->nc->remote_address);
    cl->bad++;

    return 0;

}

#define NOTHANDLED sv_netmsg_not_handled
static int (*recv_message[NUM_NETMESSAGES])(client_t *,  netmsg) = {
    NOTHANDLED, /* NETMSG_NONE */
    sv_netmsg_recv_noop,
    sv_netmsg_recv_query_version,
    sv_netmsg_recv_version,
    sv_netmsg_recv_textmessage,
    sv_netmsg_recv_quit,
    NOTHANDLED, /* NETMSG_REJECTION */
    NOTHANDLED, /* NETMSG_SV_INFO */
    NOTHANDLED, /* NETMSG_CHANGELEVEL */
    NOTHANDLED, /* NETMSG_START_FRAME */
    NOTHANDLED, /* NETMSG_END_FRAMS */
    NOTHANDLED, /* NETMSG_ENTITY */
    NOTHANDLED, /* NETMSG_MYENTITY */
    NOTHANDLED, /* NETMSG_RAILSLUG */
    NOTHANDLED, /* NETMSG_UPDATE_STATUSBAR */
    sv_netmsg_recv_join,
    sv_netmsg_recv_ready,
    sv_netmsg_recv_query_sv_info,
    sv_netmsg_recv_cl_update,
    NOTHANDLED, /* NETMSG_GAMEMESSAGE */
    NOTHANDLED, /* NETMSG_MAP_TARGET */
    NOTHANDLED, /* NETMSG_OBJECTIVE */
    sv_netmsg_recv_killme,
    sv_netmsg_recv_query_frags,
    NOTHANDLED, /* NETMSG_FRAGS */
    sv_netmsg_recv_away
};


/* Handle message and return the message type (-1 on error) */
static int sv_net_handle_message(client_t *cl, netmsg msg)
{

    if( msg.type >= NUM_NETMESSAGES ) {
	printf("sv_netmsg_recv: Unknown message type %d from %s!\n",
	       msg.type, cl->name);
	return -1;
    }

    if( recv_message[msg.type](cl, msg) < 0 )
	return NETMSG_QUIT; /* Something went wrong */
    else
	return msg.type;

}


