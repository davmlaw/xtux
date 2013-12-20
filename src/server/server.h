/* server.h
 * Global Header file for XTux Arena server
 * Created 21 Jan 2000 David Lawrence
 */

#define SERVER
/* versions are in date format, ie "ver0/ver1/ver2" d/m/y */
#define VER0 1
#define VER1 6
#define VER2 1
#define VERSION "XTux Arena server 20010601"

typedef struct {
    char name[NETMSG_STRLEN];
    int clients;
    int max_clients;
    int port;
    int fps;
    int exit_when_empty;
    int quit;
    msec_t now;
    int with_ggz;
} server_t;
