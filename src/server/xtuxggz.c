/**********************************************************************
 Functions needed to support the GGZ mode
***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>

#include "easysock.h"
#include "ggz_protocols.h"
#include "ggz_server.h"

static int ggz_sock;

int game_handle_ggz(int ggz_fd, int* p_fd)
{
    int op, seat, status = -1;
    int i;

    if (es_read_int(ggz_fd, &op) < 0)
        return -1;

    switch (op) {

    case REQ_GAME_LAUNCH:
        if (ggz_game_launch() == 0) {

            for (i = 0; i < ggz_seats_num(); i++) {
                if( ggz_seats[i].assign == GGZ_SEAT_BOT ) {
                    /* bots are not present in xtux */
                }
            }
        } else {
            perror("Error en game launch()");
        }
        status = 0;
        break;
        
    case REQ_GAME_JOIN:
        if (ggz_player_join(&seat, p_fd) == 0) {
/*          net_printf(*p_fd,TOKEN_GGZ"\n"); */
            status = 1;
        }
        break;

    case REQ_GAME_LEAVE:
        if ( (status = ggz_player_leave(&seat, p_fd)) == 0) {
/* TODO player `p_fd' leaves the game */
            status = 2;

            /*
             * if all the ggz players left the game send a
             * GAME_OVER to the ggz server
             * This fixes the `phantom table' effect.
             * TODO: Remove this after version 0.0.5 of ggz
             */
            for (i = 0; i < ggz_seats_num(); i++) {
                if( ggz_seats[i].assign == GGZ_SEAT_PLAYER )
                    break;
            }
            if(i==ggz_seats_num() ) {
                if( es_write_int(ggz_sock, REQ_GAME_OVER) < 0)
                    fprintf(stderr,"Error sending REQ_GAME_OVER\n");
            }
        }
        break;
        
    case RSP_GAME_OVER:
        status = 3; /* Signal safe to exit */
        break;

    default:
        /* Unrecognized opcode */
        status = -1;
        break;
    }

    return status;
}

int xtuxggz_find_ggzname( int fd, char *n, int len )
{
    int i;
    if(!n)
        return -1;

    for (i = 0; i < ggz_seats_num(); i++) {
        if( ggz_seats[i].fd == fd ) {
            if( ggz_seats[i].name ) {
                strncpy(n,ggz_seats[i].name,len);
                n[len]=0;
                return 0;
            } else
                return -1;
        }
    }

    return -1;
}

int xtuxggz_init(void)
{
    /* Initialize ggz */
    ggz_server_init("xtux");

    if ( (ggz_sock = ggz_server_connect()) < 0) {
        ggz_server_quit();
        fprintf(stderr,"Only the GGZ server must call Xtux server in GGZ mode!\n");
        return -1;
    }
    return ggz_sock;
}

int xtuxggz_giveme_sock(void)
{
    return ggz_sock;
}

/* return codes
 * 
 * -1: Big error!! 
 *  0: All ok, how boring!
 *  1: A player joined
 *  2: A player left
 *  3: Safe to exit 
 */
int xtuxggz_is_sock_set(int *newfd)
{
    return (game_handle_ggz(ggz_sock, newfd));
}

int xtuxggz_exit()
{
    if(ggz_sock) close(ggz_sock);
    ggz_server_quit();
    return 0;
}

int xtuxggz_avoid_perdig_effect(int sock)
{
/*  char ign[]="ignore\n"; */
/*  write(sock,ign,sizeof(ign)); */
    return 0;
}
