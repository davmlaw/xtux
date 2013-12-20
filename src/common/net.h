#include "net_msg.h"

/* Netstat types */
typedef enum {
    NS_NONE,
    NS_TOTAL,
    NS_RECENT,
    NUM_NETSTATS
} netstats_t;

typedef enum {
    NM_ALL,
    NM_FIRST_FRAME,
    NM_LAST_FRAME
} nm_frame_t;

#define NETBUFSIZ 8192

typedef struct {
    char buf[NETBUFSIZ];
    int size; /* Amount of bytes of data currently in buf */
    int pos;
    unsigned int throughput;
    /* Frame number and border positions */
    int frames;
    int start;
    int end;
} netbuf_t;

typedef struct {
    netbuf_t input;
    netbuf_t output;
    int fd;
    int record_fd; /* Record data to this file */
    int incoming; /* Used in server to indicate incoming input */ 
    int fake; /* Set when not a real net connection */
    char *remote_address;
} netconnection_t;

typedef struct {
    netshort x_tiles;
    netshort y_tiles;
    netshort fps;
} demo_header;

int net_init_socket(void);
netconnection_t *net_new_connection(int fd);
void net_close_connection(netconnection_t *nc);

/* Demo Record / play */
netconnection_t *net_rec_new_connection(int fd,char *name, demo_header header);
netconnection_t *net_fake_connection(char *demoname, demo_header *header);

/* Netconnection I/O */
int net_input_flush(netconnection_t *nc, nm_frame_t last_frame);
int net_output_flush(netconnection_t *nc);
void net_put_message(netconnection_t *nc, netmsg msg);
netmsg net_next_message(netconnection_t *nc, nm_frame_t last_frame);

vector_t net_stats(netconnection_t *nc, netstats_t type);
char *net_message_name(byte msg_type);
int net_message_size(byte type);
