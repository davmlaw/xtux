/*	$Id: ggz_client.h,v 1.1 2001/04/29 00:18:43 riq Exp $	*/
/*
 * Copyright (C) 2000 GGZ devel team
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
 *
 */
/*
 * @file ggz_client.h
 * Functions for the client
 */

#ifndef __GGZ_CLIENT_GGZ_H
#define __GGZ_CLIENT_GGZ_H

extern int ggz_client_connect( void );
extern int ggz_client_quit( void );
extern int ggz_client_init( char *game_name );
extern int ggz_client_get_sock();

#endif /* __GGZ_CLIENT_GGZ_H */
