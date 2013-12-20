/* XTux Arena Client. David Lawrence - philaw@ozemail.com.au
 * Client network code.
 * Started Jan 18 2000
 */

#include <unistd.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "xtux.h"
#include "client.h"
#include "win.h"
#include "entity.h"
#include "cl_net.h"
#include "cl_netmsg_send.h"
#include "cl_netmsg_recv.h"
#include "draw.h"

#include "ggz_client.h"

extern win_t win;
extern client_t client;

netconnection_t *server; /* Connection to the server */

void cl_network_init(void)
{

    if(!client.with_ggz) {
    	client.server_address = strdup("localhost");
    	client.port = DEFAULT_PORT;
    } else {
    	client.server_address = strdup("GGZ host");
    	client.port = 0;
    }
}


void sigcatcher(int signal)
{

    if( signal == SIGALRM )
	printf("Timed out!\n");

    return;

}


int cl_network_connect(char *sv_name, int port)
{
    struct hostent *hp;
    struct sockaddr_in addr;
    netmsg msg;
    demo_header header;
    int sock = -1;

    if( client.demo != DEMO_PLAY ) { /* A real connection, not just a demo */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if(!client.with_ggz) {
	    sock = net_init_socket();

	    if( (addr.sin_addr.s_addr = inet_addr(sv_name)) == INADDR_NONE ) {
		if((hp = gethostbyname(sv_name)) == NULL) {
		    close(sock);
		    sock = -1;
		    return 0;
		} else {
		    addr.sin_addr.s_addr = *(long *) hp->h_addr;
		}
	    }

	    if( connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0 ) {
		perror("connect");
		close(sock);
		sock = -1;
		return 0;
	    }
	} else {
	    /* ggz mode */
	    signal(SIGPIPE, SIG_IGN); /* Ignore Pipe errors */
	    if( (sock = ggz_client_get_sock()) < 0 ) {
		sock = -1;
		return 0;
	    }
	    /* avoid perdig effect */
	    sleep(3);
	}
    }

    switch( client.demo ) {
    case DEMO_RECORD:
	printf("RECORDING Connection.......\n");
	header.x_tiles = client.x_tiles;
	header.y_tiles = client.y_tiles;
	header.fps = client.fps;
	server = net_rec_new_connection(sock, client.demoname, header);
	break;
    case DEMO_PLAY:
	server = net_fake_connection(client.demoname, &header);
	client.x_tiles = header.x_tiles;
	client.y_tiles = header.y_tiles;
	client.view_w = client.x_tiles * TILE_W;
	client.view_h = client.y_tiles * TILE_H;
	break;
    case DEMO_NONE:
    default:
	server = net_new_connection(sock);
    }

    if( server == NULL ) {
	printf("Couldn't create new connection\n");
	close(sock);
	sock = -1;
	return 0;
    }

    client.connected = 1;
    client.sv_status = JOINING;

    printf("Joining Game....\n");
    cl_netmsg_send_join();
    printf("Querying Server info......\n");
    cl_netmsg_send_query_sv_info();
    net_output_flush(server);

    /* FIXME: This alarm() does not work correctly */
    alarm(10); /* Timeout after 10 secs */
    signal(SIGALRM, sigcatcher);

    printf("Reading data from server......  ");
    while( net_input_flush(server, NM_ALL) > 0 ) {
	printf("Got something!\n");
	while( (msg = net_next_message(server, NM_ALL)).type != NETMSG_NONE ) {
	    if( msg.type == NETMSG_SV_INFO ) {
		/* Got server info ok, we can join the game */
		alarm(0);
		cl_net_handle_message(msg);
		return 1;
	    } else if(msg.type == NETMSG_REJECTION || msg.type == NETMSG_QUIT){
		/* We were rejected or another error occured trying to join */
		alarm(0);
		cl_net_handle_message(msg);
		return 0;
	    } else if( client.debug ) {
		printf("IGNORING message type %d!\n", msg.type);
	    }
	}
    }

    alarm(0);
    printf("TIMED OUT waiting for server info!\n");
    cl_network_disconnect();
    return 0;

}


void cl_network_disconnect(void)
{

    if( client.connected ) {
	printf("Disconnecting\n");
	net_close_connection(server);
	client.connected = 0;
	client.health = 0;
    } else /* Not connected (shouldn't be allowed to happen) */
	printf("Not Connected!\n");

    if( client.demo == DEMO_PLAY ) { /* Put things back after demo */
	printf("Restoring.....\n");
	strncpy(client.map, "penguinland.map", NETMSG_STRLEN );
	client.demo = DEMO_NONE;
	client.view_w = client.desired_w;
	client.view_h = client.desired_h;
	client.x_tiles = client.view_w / TILE_W;
	client.y_tiles = client.view_h / TILE_H;
    }

}


void cl_net_connection_lost(char *str)
{
    if( str ) {
	clear_area(win.buf, 0, client.view_h/2, client.view_w, 32, "black");
	win_center_print(win.buf, str, client.view_h/2, 3, "white");
    } else
	win_center_print(win.buf, "Connection to server lost", 
			 client.view_h/2, 3, "white");
    win_update();
    delay(2*M_SEC);
    cl_network_disconnect();
    client.state = MENU;
}


/*
  Function pointer table for handling incoming messages.
  All of the below functions are in "cl_netmsg_recv.c"
  We shouldn't get the messages that are NOTHANDLED, because
  the server shouldn't send them. In theory anyway.
*/
static void cl_netmsg_not_handled(netmsg msg)
{
    printf("NOT HANDLING MESSAGE \"%s\"\n", net_message_name(msg.type));
}


#define NOTHANDLED cl_netmsg_not_handled
void (*recv_message[NUM_NETMESSAGES])(netmsg) = {
    NOTHANDLED, /* NETMSG_NONE */
    NOTHANDLED, /* NETMSG_NOOP */
    cl_netmsg_recv_query_version,
    cl_netmsg_recv_version,
    cl_netmsg_recv_textmessage,
    cl_netmsg_recv_quit,
    cl_netmsg_recv_rejection,
    cl_netmsg_recv_sv_info,
    cl_netmsg_recv_changelevel,
    cl_netmsg_recv_start_frame,
    cl_netmsg_recv_end_frame,
    cl_netmsg_recv_entity,
    cl_netmsg_recv_myentity,
    cl_netmsg_recv_particles,
    cl_netmsg_recv_update_statusbar,
    NOTHANDLED,  /* NETMSG_JOIN */
    NOTHANDLED,  /* NETMSG_READY */
    NOTHANDLED,  /* NETMSG_QUERY_SV_INFO */
    NOTHANDLED,  /* NETMSG_CL_UPDATE */
    cl_netmsg_recv_gamemessage,
    cl_netmsg_recv_map_target,
    cl_netmsg_recv_objective,
    NOTHANDLED,  /* NETMSG_KILLME */
    NOTHANDLED,  /* NETMSG_QUERY_FRAGS */
    cl_netmsg_recv_frags,
    NOTHANDLED   /* NETMSG_AWAY */
};
    

/* Handle message and return the message type */
int cl_net_handle_message(netmsg msg)
{

    if( client.debug )
	printf("cl_net_handle_message: type = %s\n",
	       net_message_name(msg.type));

    if( msg.type < NUM_NETMESSAGES ) {
	if( recv_message[msg.type] == NULL )
	    cl_netmsg_not_handled(msg);
	else
	    recv_message[msg.type](msg);
	return msg.type;
    } else {
	printf("cl_netmsg_recv: Unknown message type %d!\n", msg.type);
	return -1;
    }

}


int cl_net_update(void)
{
    int status;
    nm_frame_t nm;
    netmsg msg;

    if( client.debug ) {
	printf("**************************\n");
	printf("cl_net_update(): About to get read() from server.....\n");
    }

    if( client.demo == DEMO_PLAY )
	nm = NM_FIRST_FRAME;
    else
	nm = NM_LAST_FRAME;

    status = net_input_flush( server, nm );
    if( status < 0 ) { /* Check for errors */
	if( client.demo == DEMO_PLAY ) { /* END of the demo. */
	    cl_network_disconnect();
	    client.state = MENU;
	} else {
	    printf("cl_net_update(): net_input_flush() returned %d\n", status);
	    cl_net_connection_lost(NULL);
	}
	return status;
    } else if( status == 0 ) {
	printf("Skipping this loop, but trying again\n");
	return status;
    }

    while( (msg = net_next_message(server,NM_LAST_FRAME)).type != NETMSG_NONE )
	cl_net_handle_message(msg);

    return status;

}


void cl_net_finish(void)
{

    net_output_flush(server);

}


void cl_net_stats(void)
{
    static vector_t NS; /* Network stats */
    static time_t t_last;

    time_t t_now;

    t_now = time(NULL);
    /* Only update net stats once a second */
    if( t_now != t_last ) {
	t_last = t_now;
	NS = net_stats(server, client.netstats);
    }
    draw_netstats(NS);

}



