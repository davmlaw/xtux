#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

#include "xtux.h"

#define HTONS(a) (a = htons(a))
#define NTOHS(a) (a = ntohs(a))

static struct {
    char *name;
    int size;
    int frame_constrained;
} netmsg_table[NUM_NETMESSAGES] = {
    { DFNTOSTR( NETMSG_NONE ),             sizeof(netmsg_none),             0},
    { DFNTOSTR( NETMSG_NOOP ),             sizeof(netmsg_noop),             0},
    { DFNTOSTR( NETMSG_QUERY_VERSION ),    sizeof(netmsg_query_version),    0},
    { DFNTOSTR( NETMSG_VERSION ),          sizeof(netmsg_version),          0},
    { DFNTOSTR( NETMSG_TEXTMESSAGE ),      sizeof(netmsg_textmessage),      0},
    { DFNTOSTR( NETMSG_QUIT ),             sizeof(netmsg_quit),             0},
    { DFNTOSTR( NETMSG_REJECTION ),        sizeof(netmsg_rejection),        0},
    { DFNTOSTR( NETMSG_SV_INFO ),          sizeof(netmsg_sv_info),          0},
    { DFNTOSTR( NETMSG_CHANGELEVEL ),      sizeof(netmsg_changelevel),      0},
    { DFNTOSTR( NETMSG_START_FRAME ),      sizeof(netmsg_start_frame),      1},
    { DFNTOSTR( NETMSG_END_FRAME ),        sizeof(netmsg_end_frame),        1},
    { DFNTOSTR( NETMSG_ENTITY ),           sizeof(netmsg_entity),           1},
    { DFNTOSTR( NETMSG_MYENTITY ),         sizeof(netmsg_entity),           1},
    { DFNTOSTR( NETMSG_PARTICLES ),        sizeof(netmsg_particles),        0},
    { DFNTOSTR( NETMSG_UPDATE_STATUSBAR ), sizeof(netmsg_update_statusbar), 0},
    { DFNTOSTR( NETMSG_JOIN ),             sizeof(netmsg_join),             0},
    { DFNTOSTR( NETMSG_READY ),            sizeof(netmsg_ready),            0},
    { DFNTOSTR( NETMSG_QUERY_SV_INFO ),    sizeof(netmsg_query_sv_info),    0},
    { DFNTOSTR( NETMSG_CL_UPDATE ),        sizeof(netmsg_cl_update),        0},
    { DFNTOSTR( NETMSG_GAMEMESSAGE ),      sizeof(netmsg_gamemessage),      0},
    { DFNTOSTR( NETMSG_MAP_TARGET ),       sizeof(netmsg_map_target),       0},
    { DFNTOSTR( NETMSG_OBJECTIVE ),        sizeof(netmsg_objective),        0},
    { DFNTOSTR( NETMSG_KILLME ),           sizeof(netmsg_killme),           0},
    { DFNTOSTR( NETMSG_QUERY_FRAGS ),      sizeof(netmsg_query_frags),      0},
    { DFNTOSTR( NETMSG_FRAGS ),            sizeof(netmsg_frags),            0},
    { DFNTOSTR( NETMSG_AWAY ),             sizeof(netmsg_away),             0}
};

static char *net_remote_address(int fd);
static void net_mark_full_frame(netbuf_t *in, nm_frame_t nm);

/* Returns fd initialised in socket(), -1 on error */
int net_init_socket(void)
{
    int sock;

    signal(SIGPIPE, SIG_IGN); /* Ignore Pipe errors */
    if( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	perror("socket");

    return sock;

}


static void net_connection_reset(netconnection_t *nc)
{
    nc->input.size = 0;
    nc->input.pos = 0;
    nc->input.throughput = 0;

    nc->output.size = 0;
    nc->output.pos = 0;
    nc->output.throughput = 0;
}


static netconnection_t *nc_alloc(void)
{
    netconnection_t *nc;

    if( (nc = malloc( sizeof(netconnection_t) )) == NULL )
	perror("malloc");
    return nc;

}

/* Returns a new netconnection (Input and Output buffers) for a
   socket connection on fd */
netconnection_t *net_new_connection(int fd)
{
    netconnection_t *nc;

    if( (nc = nc_alloc()) ) {
	net_connection_reset(nc);
	nc->fd = fd;
	nc->record_fd = 0;
	nc->incoming = 0;
	nc->fake = 0;
	nc->remote_address = net_remote_address(fd);
    }

    return nc;

}


void net_close_connection(netconnection_t *nc)
{

    if( nc ) {
	net_output_flush(nc);
	close(nc->fd);
	if( nc->record_fd )
	    close(nc->record_fd);
	free(nc);
    }

}


/* New connection, but record it to name */
netconnection_t *net_rec_new_connection(int fd, char *name, demo_header header)
{
    int size, demo_fd;
    netconnection_t *nc;

    if( !(demo_fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0644)) ) {
	perror("open");
	return NULL;
    }

    HTONS( header.x_tiles );
    HTONS( header.y_tiles );
    HTONS( header.fps );

    size = write( demo_fd, &header, sizeof(demo_header));
    if( size != sizeof(demo_header) ) {
	printf("Couldn't write header information from \"%s\"\n", name);
	if( size < 0 )
	    perror("write");
	else
	    printf("Wrote only %d bytes of %d.\n", size, sizeof(demo_header));

	close(demo_fd);
	return NULL;
    }

    if( (nc = net_new_connection(fd)) )
	nc->record_fd = demo_fd;
    else
	close(demo_fd);

    return nc;

}


static char *fake_address = "fake_address";

/* Start a fake net connection (used for demos on CLIENT) */
netconnection_t *net_fake_connection(char *demoname, demo_header *header)
{
    int fd;
    ssize_t size;
    netconnection_t *nc;

    if( !(fd = open(demoname, O_RDONLY)) ) {
	perror("open");
	return NULL;
    }

    size = read(fd, header, sizeof(demo_header));
    if( size != sizeof(demo_header) ) {
	printf("Couldn't Read header information from \"%s\"\n", demoname);
	if( size < 0 )
	    perror("read");
	else
	    printf("Read only %d bytes of %d.\n", size, sizeof(demo_header));

	close(fd);
	return NULL;
    }

    NTOHS( header->x_tiles );
    NTOHS( header->y_tiles );
    NTOHS( header->fps );

    if( (nc = nc_alloc()) ) {
	net_connection_reset(nc);
	nc->fd = fd;
	nc->record_fd = 0;
	nc->fake = 1;
	nc->remote_address = fake_address;
    }

    return nc;

}


/*
  Read from the network in netconnection and copy to input buffer.
  If last_frame is 1, then ignore certain types of messages that occur
  outside of the last frame
  Return codes: -1 on error
                 0 on recoverable error.
                 1 on success
*/
int net_input_flush(netconnection_t *nc, nm_frame_t nm)
{
    int offset;
    int recv_size;

    /* offset = amount of bytes left over in netbuf from last time */
    offset = nc->input.size - nc->input.pos;

    if( offset < 0 )
	offset = 0;
    else if( offset > NETBUFSIZ ) {
	printf("Offset too big!\n");
	offset = 0;
    } else if( offset > 0 && nc->input.pos > 0 )
	memmove( nc->input.buf, nc->input.buf + nc->input.pos, offset );


    recv_size = read( nc->fd, nc->input.buf + offset, NETBUFSIZ - offset);

    if( recv_size == 0 )
	return -1; /* EOF */
    else if( recv_size < 0 ) {
	perror("read");
	if( errno == EAGAIN )
	    return 0; /* Error, but recoverable and we should try again */
	else
	    return -1;
    }

    if( nc->record_fd ) /* Record last read */
	write( nc->record_fd, nc->input.buf + offset, recv_size ); 

    nc->input.throughput += recv_size;
    nc->input.size = offset + recv_size;
    nc->input.pos = 0;

    /* Default frame variables */
    nc->input.frames = 0;
    nc->input.start = 0;
    nc->input.end = nc->input.size;

    if( nm )
	net_mark_full_frame(&nc->input, nm);

    return 1;

}



/* Flush the output buffer and send it off across the network */
int net_output_flush(netconnection_t *nc)
{
    int written = 0; /* bytes written by write() */
    int left; /* Bytes left to send */

    if( nc->fake ) { /* Fake, so no output */
	nc->output.size = 0;
	return 0;
    }

    if( nc->output.size ) {
	written = write(nc->fd, nc->output.buf, nc->output.size);

	if( written < 0 ) {
	    if( errno == EAGAIN || errno == EBUSY )
		return 0;
	    else {
		perror("write");
		return -1;
	    }
	}

	left = nc->output.size - written;

	if( left > 0 ) { /* Still some bytes to write */
	    memmove( nc->output.buf, nc->output.buf + written, left );
	}

	nc->output.throughput += written;
	nc->output.size = left;
    }

    return written;

}


/* Find and mark the first/last complete frame in input buffer,
   if there is only a partial frame, then set in->frames to -1 */
static void net_mark_full_frame(netbuf_t *in, nm_frame_t nm)
{
    int start_found;
    int p, type, size;

    start_found = -1;

    for( p = in->pos ; p < in->size ; p += size ) {
	type = (byte)in->buf[p];
	if( type >= NUM_NETMESSAGES ) {
	    printf("ERROR: Type (%d) out of range!\n", type);
	    printf("in->pos = %d, in->size = %d, p = %d\n",
		   in->pos, in->size, p);
	    in->size = 0;
	    return;
	}
	size = netmsg_table[type].size;

#if NET_DEBUG
	printf("%4d: (%3d), \"%s\" (s=%d)\n",
	       p, type, net_message_name(type), size);
#endif

	if( type == NETMSG_START_FRAME ) {
	    start_found = p;
	    if( in->frames == 0 )
		in->frames = -1; /* Partial frame */
	} else if( type == NETMSG_END_FRAME && start_found >= 0 ) {
	    /* Ok, we now have a full frame */
	    in->start = start_found;
	    in->end = p + size;
	    in->frames++;
	    if( nm == NM_FIRST_FRAME )
		return;
	}
    }

}


static void tonet(netmsg *msg);
static void fromnet(netmsg *msg);

/* Put a message onto the output buffer */
void net_put_message(netconnection_t *nc, netmsg msg)
{
    int type, size;

#if NET_DEBUG
    printf("net_put_message(%s)\n", net_message_name(msg.type));
#endif

    type = (int)msg.type;
    if( type >= NUM_NETMESSAGES ) {
	printf("Unknown message type %d\n", type);
	return;
    }

    size = netmsg_table[type].size;

    if( nc->output.size + size > NETBUFSIZ ) {
	printf("Output full. Auto-flushing.\n");
	net_output_flush(nc);

	if( nc->output.size + size > NETBUFSIZ ) {
	    printf("Could not write output to %s. Discarding buffer\n",
		   nc->remote_address);
	    nc->output.size = 0;
	    return;
	}
    }

    tonet(&msg); /* To network byte order */
    memcpy(nc->output.buf + nc->output.size, &msg, size);
    nc->output.size += size;

}


/* Return the next netmsg from the input buffer or NETMSG_NONE if none. */
netmsg net_next_message(netconnection_t *nc, nm_frame_t last_frame)
{
    netbuf_t *in;
    netmsg msg;
    int p;
    int type, size; /* Message details */

    in = &nc->input; /* Small variable name for convenience */
    msg.type = NETMSG_NONE; /* Early returns (errors) will be NETMSG_NONE */

    if( last_frame == NM_LAST_FRAME && in->frames == -1 ) {
	printf("Waiting for the rest of frame......\n");
	return msg;
    }

    if( in->end - in->start <= 0 ) { /* Nothing in input buffer */
	printf("in->start = %d, in->end = %d!\n", in->start, in->end);
	return msg; /* NONE */
    }

    /* Go from beginning of input buffer, if messages we want to discard
       lie outside of the boundaries, then on to the next message */

    for( p = in->pos ; p < in->end ; p += size ) {
	type = (byte)in->buf[p];

	/* Fixes DoS attack. Added Jan 11 2003 David Lawrence */
	if( type >= NUM_NETMESSAGES ) {
	    printf("Net: Unknown message type from client.\n");
	    return msg;
	}

	size = netmsg_table[type].size;
	in->pos += size;

	if( p < in->start && netmsg_table[type].frame_constrained )
	    continue; /* Drop in frame message up till current frame. */

	if( size > 0 && in->end - p >= size ) {
	    memcpy( &msg, in->buf + p, size );
	    msg.type = type;
	    break;
	}



    }

    fromnet(&msg); /* To host byte order */
    return msg;

}


/* Returns the name of the message or UNKNOWN MESSAGE if type is unknown */
char *net_message_name(byte type)
{

    if( type < NUM_NETMESSAGES )
	return netmsg_table[(int)type].name;
    else
	return "UNKNOWN MESSAGE!";

}


int net_message_size(byte type)
{
    if( type < NUM_NETMESSAGES )
	return netmsg_table[(int)type].size;
    else
	return -1;
}


/* Gets the IP address of the connection then returns the hostname if it can
   resolve it, otherwise it returns the ip address in ascii dot notation */
static char *net_remote_address(int fd)
{
    struct sockaddr_in sin;
    struct hostent *hp;
    int len;
    
    len = sizeof(sin);
    if( getpeername(fd, (struct sockaddr *) &sin, (socklen_t *) &len) == 0 ) {
	hp = gethostbyaddr((char*) &sin.sin_addr.s_addr,
			   sizeof(sin.sin_addr.s_addr), AF_INET);
	/* Return hostname, or IP address if we can't resolve hostname */
	if (hp == NULL)
	    return (char *)inet_ntoa(sin.sin_addr); /* IP address */
	else
	    return hp->h_name; /* Hostname */

    } else {
	perror("getpeername");
	return NULL;
    }

}


/* Returns a vector (x=input, y=output) of netstats, either total (NS_TOTAL) or
   averaged over the last second (NS_RECENT) depending on value time_frame */
vector_t net_stats(netconnection_t *nc, netstats_t time_frame)
{
    static msec_t last_time;
    static unsigned long last_in = 0, last_out = 0;
    msec_t now;
    float tick;
    vector_t NS;

    NS.x = nc->input.throughput;
    NS.y = nc->output.throughput;

    if( time_frame == NS_RECENT ) {
	/* RECENT: NS = Bytes moved since last call */
	NS.x -= last_in;
	NS.y -= last_out;

	last_in = nc->input.throughput;
	last_out = nc->output.throughput;

	/* Avg bytes moved over during a second */
	now = gettime();
	tick = M_SEC / (now - last_time);
	NS.x *= tick;
	NS.y *= tick;
	last_time = now;
    }

    return NS;

}


/* Any net-message that has an integer data type larger than a byte must be
   converted to and from network byte order! */

static void tonet(netmsg *msg)
{

    switch( msg->type ) {
    case NETMSG_READY:
	HTONS(msg->ready.view_w);
	HTONS(msg->ready.view_h);
	break;
    case NETMSG_START_FRAME:
	HTONS(msg->start_frame.screenpos.x);
	HTONS(msg->start_frame.screenpos.y);
	break;
    case NETMSG_ENTITY:
    case NETMSG_MYENTITY:
	HTONS(msg->entity.x);
	HTONS(msg->entity.y);
	break;
    case NETMSG_PARTICLES:
	HTONS(msg->particles.x);
	HTONS(msg->particles.y);
	break;
    case NETMSG_UPDATE_STATUSBAR:
	HTONS(msg->update_statusbar.frags);
	HTONS(msg->update_statusbar.ammo);
	break;
    case NETMSG_MAP_TARGET:
	HTONS(msg->map_target.target.x);
	HTONS(msg->map_target.target.y);
	break;
    case NETMSG_FRAGS:
	HTONS(msg->frags.frame);
	HTONS(msg->frags.frags);
	break;
    default:
	break;
    }

}


static void fromnet(netmsg *msg)
{

    switch( msg->type ) {
    case NETMSG_READY:
	NTOHS(msg->ready.view_w);
	NTOHS(msg->ready.view_h);
	break;
    case NETMSG_START_FRAME:
	NTOHS(msg->start_frame.screenpos.x);
	NTOHS(msg->start_frame.screenpos.y);
	break;
    case NETMSG_ENTITY:
    case NETMSG_MYENTITY:
	NTOHS(msg->entity.x);
	NTOHS(msg->entity.y);
	break;
    case NETMSG_PARTICLES:
	NTOHS(msg->particles.x);
	NTOHS(msg->particles.y);
	break;
    case NETMSG_UPDATE_STATUSBAR:
	NTOHS(msg->update_statusbar.frags);
	NTOHS(msg->update_statusbar.ammo);
	break;
    case NETMSG_MAP_TARGET:
	NTOHS(msg->map_target.target.x);
	NTOHS(msg->map_target.target.y);
	break;
    case NETMSG_FRAGS:
	NTOHS(msg->frags.frame);
	NTOHS(msg->frags.frags);
	break;
    default:
	break;
    }

}
