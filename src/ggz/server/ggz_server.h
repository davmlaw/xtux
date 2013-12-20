/*	$Id: ggz_server.h,v 1.2 2001/04/29 00:57:09 riq Exp $	*/
/*
 * File: ggz.h
 * Author: Brent Hendricks
 * Project: GGZ 
 * Date: 3/35/00
 * Desc: GGZ game module functions
 *
 * Copyright (C) 2000 Brent Hendricks.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#ifndef __GGZ_SERVER_GGZ_H
#define __GGZ_SERVER_GGZ_H

#define MAX_USER_NAME_LEN 16

/* Seat assignment values */
#define GGZ_SEAT_OPEN   -1
#define GGZ_SEAT_BOT    -2
#define GGZ_SEAT_RESV   -3
#define GGZ_SEAT_NONE   -4
#define GGZ_SEAT_PLAYER -5

/* Debug levels for ggz_debug*/
#define GGZ_DBG_CONFIGURATION	(unsigned) 0x00000001
#define GGZ_DBG_PROCESS		(unsigned) 0x00000002
#define GGZ_DBG_CONNECTION	(unsigned) 0x00000004
#define GGZ_DBG_CHAT		(unsigned) 0x00000008
#define GGZ_DBG_TABLE		(unsigned) 0x00000010
#define GGZ_DBG_PROTOCOL	(unsigned) 0x00000020
#define GGZ_DBG_UPDATE		(unsigned) 0x00000040
#define GGZ_DBG_MISC		(unsigned) 0x00000080
#define GGZ_DBG_ROOM		(unsigned) 0x00000100
#define GGZ_DBG_LISTS		(unsigned) 0x00000200
#define GGZ_DBG_GAME_MSG	(unsigned) 0x00000400
#define GGZ_DBG_ALL		(unsigned) 0xFFFFFFFF

struct ggz_seat_t {
	int assign;
	char name[MAX_USER_NAME_LEN +1];
	int fd;
};

extern struct ggz_seat_t* ggz_seats;

/* Setup functions */
int ggz_server_init(char* game_name);
int ggz_server_connect(void);
int ggz_server_done(void);
void ggz_server_quit(void);

/* Game functions */
int ggz_game_launch(void);
int ggz_game_over(void);

/* Player functions */
int ggz_player_join(int* seat, int *fd);
int ggz_player_leave(int* seat, int *fd);

/* Useful functions */
void ggz_debug(const char *fmt, ...);

int ggz_seats_open(void);
int ggz_seats_num(void);
int ggz_seats_bot(void);
int ggz_seats_reserved(void);
int ggz_seats_human(void);

int ggz_fd_max(void);

#endif /* __GGZ_SERVER_GGZ_H */
