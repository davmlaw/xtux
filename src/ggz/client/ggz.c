/*	$Id: ggz.c,v 1.3 2001/04/29 00:18:43 riq Exp $	*/
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
 * @file ggz.c
 * Funciones de ggz para el cliente
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static char *name=NULL;
static int ggz_sock;

/**
 * @fn void ggz_connect(void)
 * Se conecta al ggz client
 */
int ggz_client_connect(void)
{
	char fd_name[80];
        struct sockaddr_un addr;
	int fd;

	if(!name) {
		fprintf(stderr,"ggz is not initialized!\n");
		return -1;
	}
	
	/* Connect to Unix domain socket */
	snprintf(fd_name, sizeof(fd_name)-1, "/tmp/%s.%d", name, getpid());
	fd_name[sizeof(fd_name)-1]=0;

	if ( (fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		perror("ggz_connect:");
		return -1;
	}

	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, fd_name,sizeof(addr.sun_path)-1);
	addr.sun_path[sizeof(addr.sun_path)-1]=0;

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("ggz_connect:");
		return -1;
	}

	ggz_sock = fd;
	return fd;
}

int ggz_client_init(char *game_name)
{
	int len;

	if(name) {
		fprintf(stderr,"ggz is already initialized");
		return 0;
	}

	len = strlen(game_name);
	if ( (name = malloc(len+1)) == NULL)
		return -1;
	strcpy(name, game_name);

	ggz_sock=-1;
	
	return 0;
}

int ggz_client_quit()
{
	if(name) free(name);
	return 0;
}

int ggz_client_get_sock()
{
	return ggz_sock;
}
