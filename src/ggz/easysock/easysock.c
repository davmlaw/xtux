/*
 * File: easysock.c
 * Author: Brent Hendricks
 * Project: libeasysock
 * Date: 4/16/98
 *
 * A library of useful routines to make life easier while using 
 * sockets
 *
 * Copyright (C) 1998 Brent Hendricks.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifdef DEBUG_MEM
# include <dmalloc.h>
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#include "easysock.h"

#define SA struct sockaddr  

static es_err_func _err_func = NULL;
static es_exit_func _exit_func = exit;
static unsigned int es_alloc_limit = 32768;

static void _debug(const char *fmt, ...)
{
#ifdef DEBUG_SOCKET
	char buf[4096];
	va_list ap;

	va_start(ap, fmt);
	sprintf(buf, "[%d]: ", getpid());
	vsprintf(buf + strlen(buf), fmt, ap);
	
	fputs(buf, stderr);
	va_end(ap);
#endif
}


int es_err_func_set(es_err_func func)
{
	_err_func = func;
	return 0;
}


es_err_func es_err_func_rem(void)
{
	es_err_func old = _err_func;
	_err_func = NULL;
	return old;
}


int es_exit_func_set(es_exit_func func)
{
	_exit_func = func;
	return 0;
}


es_exit_func es_exit_func_rem(void)
{
	es_exit_func old = _exit_func;
	_exit_func = exit;
	return old;
}


unsigned int es_alloc_limit_get(void)
{
	return es_alloc_limit;
}


unsigned int es_alloc_limit_set(const unsigned int limit)
{
	unsigned int old = es_alloc_limit;
	es_alloc_limit = limit;
	return old;
}


int es_make_socket(const EsSockType type, const unsigned short port, 
		   const char *server)
{
	int sock;
	const int on = 1;
#ifdef INET6
	struct sockaddr_in6 name;
#else
	struct sockaddr_in name;
#endif
	struct hostent *hp;

#ifdef INET6
	if ( (sock = socket(PF_INET6, SOCK_STREAM, 0)) < 0)
#else
	if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
#endif
	{
		if (_err_func)
			(*_err_func) (strerror(errno), ES_CREATE, ES_NONE);
		return -1;
	}

	memset(&name, 0, sizeof(name));

#ifdef INET6
	name.sin6_family = AF_INET6;
	name.sin6_port = htons(port);
#else
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
#endif

	switch (type) {

	case ES_SERVER:
#ifdef INET6
		name.sin6_addr=in6addr_any;
#else
		name.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&on, 
			       sizeof(on)) < 0
		    || bind(sock, (SA *)&name, sizeof(name)) < 0) {
			if (_err_func)
				(*_err_func) (strerror(errno), ES_CREATE, 
					      ES_NONE);
			return -1;
		}
		break;

	case ES_CLIENT:
#ifdef INET6
		if( inet_pton( AF_INET6, server, &name.sin6_addr ) <= 0 &&
			inet_pton(AF_INET, server, &name.sin6_addr) )
#else
		if ( (hp = gethostbyname(server)) == NULL)
#endif
		{
			if (_err_func)
				(*_err_func) ("Lookup failure", ES_CREATE, 
					      ES_NONE);
			return -1;
			break;
		}
#ifndef INET6
		memcpy(&name.sin_addr, hp->h_addr, hp->h_length);
#endif
		if (connect(sock, (SA *)&name, sizeof(name)) < 0) {
			if (_err_func)
				(*_err_func) (strerror(errno), ES_CREATE, 
					      ES_NONE);
			return -1;
		}
		break;
	}

	return sock;
}


int es_make_socket_or_die(const EsSockType type, const unsigned short port, 
			  const char *server)
{
	int sock;
	
	if ( (sock = es_make_socket(type, port, server)) < 0)
		(*_exit_func) (-1);
	
	return sock;
}


int es_make_unix_socket(const EsSockType type, const char* name) 
{
	int sock;
	struct sockaddr_un addr;
	
	if ( (sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		if (_err_func)
			(*_err_func) (strerror(errno), ES_CREATE, ES_NONE);
		return -1;
	}

	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	/* Copy in filename up to the limit, leaving room for \0 term. */
	strncpy(addr.sun_path, name, (sizeof(addr.sun_path) - 1));

	switch(type) {

	case ES_SERVER:
		unlink(name);
		if (bind(sock, (SA *)&addr, SUN_LEN(&addr)) < 0 ) {
			if (_err_func)
				(*_err_func) (strerror(errno), ES_CREATE, 
					      ES_NONE);
			return -1;
		}
		break;
	case ES_CLIENT:
		if (connect(sock, (SA *)&addr, sizeof(addr)) < 0) {
			if (_err_func)
				(*_err_func) (strerror(errno), ES_CREATE, 
					      ES_NONE);
			return -1;
		}
		break;
	}
	return sock;
}


int es_make_unix_socket_or_die(const EsSockType type, const char* name) 
{
	int sock;
	
	if ( (sock = es_make_unix_socket(type, name)) < 0)
		(*_exit_func) (-1);
	
	return sock;
}


int es_write_char(const int sock, const char message)
{
	if (es_writen(sock, &message, sizeof(char)) < 0) {
		_debug("Error sending char\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_WRITE, ES_CHAR);
		return -1;
	}

	_debug("Sent \"%d\" : char\n", message);
	return 0;
}


void es_write_char_or_die(const int sock, const char data)
{
	if (es_write_char(sock, data) < 0)
		(*_exit_func) (-1);
}


int es_read_char(const int sock, char *message)
{
	int status;

	if ( (status = es_readn(sock, message, sizeof(char))) < 0) {
		_debug("Error receiving char\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_READ, ES_CHAR);
		return -1;
	} 

	if (status < sizeof(char)) {
		_debug("Warning: fd is closed\n");
		if (_err_func)
			(*_err_func) ("fd closed", ES_READ, ES_CHAR);
		return -1;
	} 
	
	_debug("Received \"%d\" : char\n", *message);
	return 0;
}


void es_read_char_or_die(const int sock, char *data)
{
	if (es_read_char(sock, data) < 0)
		(*_exit_func) (-1);
}


/*
 * Take a host-byte order int and write on fd using
 * network-byte order.
 */
int es_write_int(const int sock, const int message)
{
	int data = htonl(message);

	if (es_writen(sock, &data, sizeof(int)) < 0) {
		_debug("Error sending int\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_WRITE, ES_INT);
		return -1;
	}

	_debug("Sent \"%d\" : int\n", message);
	return 0;
}


void es_write_int_or_die(const int sock, const int data)
{
	if (es_write_int(sock, data) < 0)
		(*_exit_func) (-1);
}


/*
 * Read a network byte-order integer from the fd and
 * store it in host-byte order.
 */
int es_read_int(const int sock, int *message)
{
	int data, status;
	
	if ( (status = es_readn(sock, &data, sizeof(int))) < 0) {
		_debug("Error receiving int\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_READ, ES_INT);
		return -1;
	}
	
	if (status < sizeof(int)) {
		_debug("Warning: fd is closed\n");
		if (_err_func)
			(*_err_func) ("fd closed", ES_READ, ES_INT);
		return -1;
	}
	
	*message = ntohl(data);
	_debug("Received \"%d\" : int\n", *message);
	return 0;
}


void es_read_int_or_die(const int sock, int *data)
{
	if (es_read_int(sock, data) < 0)
		(*_exit_func) (-1);
}


/*
 * Write a char string to the given fd preceeded by its size
 */
int es_write_string(const int sock, const char *message)
{
	unsigned int size = strlen(message) * sizeof(char) + 1;
	
	if (es_write_int(sock, size) < 0)
		return -1;
	
	if (es_writen(sock, message, size) < 0) {
		_debug("Error sending string\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_WRITE, ES_STRING);
		return -1;
	}
	
	_debug("Sent \"%s\" : string \n", message);
	return 0;
}


int es_va_write_string(const int sock, const char *fmt, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	return es_write_string(sock, buf);
}


void es_write_string_or_die(const int sock, const char *data)
{
	if (es_write_string(sock, data) < 0)
		(*_exit_func) (-1);
}


void es_va_write_string_or_die(const int sock, const char *fmt, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	es_write_string_or_die(sock, buf);
}


/*
 * Read a char string from the given fd
 */
int es_read_string(const int sock, char *message, const unsigned int len)
{
	unsigned int size;
	int status;

	if (es_read_int(sock, &size) < 0)
		return -1;
	
	if (size > len) {
		_debug("String too long for buffer\n");
		if (_err_func)
			(*_err_func) ("String too long", ES_READ, ES_STRING);
		return -1;
	}
	
       	if ( (status = es_readn(sock, message, size)) < 0) {
		_debug("Error receiving string\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_READ, ES_STRING);
		return -1;
	} 

	if (status < size) {
		_debug("Warning: fd is closed\n");
		if (_err_func)
			(*_err_func) ("fd closed", ES_READ, ES_STRING);
		return -1;
	} 
	
	/* Guarantee NULL-termination */
	message[len-1] = '\0';
	
	_debug("Received \"%s\" : string\n", message);
	return 0;
}


void es_read_string_or_die(const int sock, char *data, const unsigned int len)
{
	if (es_read_string(sock, data, len) < 0)
		(*_exit_func) (-1);
}


/*
 * Read a char string from the given fd and allocate space for it.
 */
int es_read_string_alloc(const int sock, char **message)
{
	unsigned int size;
	int status;

	if (es_read_int(sock, &size) < 0)
		return -1;

	if (size > es_alloc_limit) {
		_debug("Error: Easysock allocation limit exceeded\n");
		if (_err_func)
			(*_err_func) ("Allocation limit exceeded", ES_ALLOCATE,
				      ES_STRING);
		return -1;
	}
	
	if ( (*message = calloc((size+1), sizeof(char))) == NULL) {
		_debug("Error: Not enough memory\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_ALLOCATE, ES_STRING);
		return -1;
	}

	if ( (status = es_readn(sock, *message, size)) < 0) {
		_debug("Error receiving string\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_READ, ES_STRING);
		return -1;
	}

	if (status < size) {
		_debug("Warning: fd is closed\n");
		if (_err_func)
			(*_err_func) ("fd closed", ES_READ, ES_STRING);
		return -1;
	} 

	_debug("Received \"%s\" : string\n", *message);
	return 0;
}


void es_read_string_alloc_or_die(const int sock, char **data)
{
	if (es_read_string_alloc(sock, data) < 0)
		(*_exit_func) (-1);
}


/* Write "n" bytes to a descriptor. */
int es_writen(int fd, const void *vptr, size_t n)
{

	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			if (errno == EINTR)
				nwritten = 0;	/* and call write() again */
			else
				return (-1);	/* error */
		}
		
		nleft -= nwritten;
		ptr += nwritten;
	}
	_debug("Wrote %d bytes\n", n);
	return (n);
}


/* Read "n" bytes from a descriptor. */
int es_readn(int fd, void *vptr, size_t n)
{

	size_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;	/* and call read() again */
			else
				return (-1);
		} else if (nread == 0)
			break;	/* EOF */

		nleft -= nread;
		ptr += nread;
	}
	_debug("Read %d bytes\n", (n - nleft));
	return (n - nleft);	/* return >= 0 */
}


int es_write_fd(int fd, int sendfd)
{
        int status;
        struct msghdr msg;
	struct iovec iov[1];

#ifdef	HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr *cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = sendfd;
#else
	msg.msg_accrights = (caddr_t) &sendfd;
	msg.msg_accrightslen = sizeof(int);
#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

        /* We're just sending a fd, so it's a dummy byte */
        iov[0].iov_base = "";
	iov[0].iov_len = 1;

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if (sendmsg(fd, &msg, 0) < 0) {
		_debug("Error sending fd\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_WRITE, ES_FD);
		return -1;
	}

	_debug("Sent \"%d\" : fd\n", sendfd);
	return 0;
}


int es_read_fd(int fd, int *recvfd)
{
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t	n;
	int newfd;
        char dummy;
  
#ifdef	HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr *cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
#else
	msg.msg_accrights = (caddr_t) &newfd;
	msg.msg_accrightslen = sizeof(int);
#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

        /* We're just sending a fd, so it's a dummy byte */
	iov[0].iov_base = &dummy;
	iov[0].iov_len = 1;

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if ( (n = recvmsg(fd, &msg, 0)) < 0) {
		_debug("Error reading fd msg\n");
		if (_err_func)
			(*_err_func) (strerror(errno), ES_READ, ES_FD);
		return -1;
	}

        if (n == 0) {
		_debug("Warning: fd is closed\n");
		if (_err_func)
			(*_err_func) ("fd closed", ES_READ, ES_FD);
	        return -1; 
	}
  
#ifdef	HAVE_MSGHDR_MSG_CONTROL
        if ( (cmptr = CMSG_FIRSTHDR(&msg)) == NULL 
	     || cmptr->cmsg_len != CMSG_LEN(sizeof(int))) {
		_debug("Bad cmsg\n");
		if (_err_func)
			(*_err_func) ("Bad cmsg", ES_READ, ES_FD);
		return -1;
	}

	if (cmptr->cmsg_level != SOL_SOCKET) {
		_debug("Bad cmsg\n");
		if (_err_func)
			(*_err_func) ("level != SOL_SOCKET", ES_READ, ES_FD);
		return -1;
	}

	if (cmptr->cmsg_type != SCM_RIGHTS) {
		_debug("Bad cmsg\n");
		if (_err_func)
			(*_err_func) ("type != SOL_RIGHTS", ES_READ, ES_FD);
		return -1;
	}

	/* Everything is good */
	*recvfd = *((int *) CMSG_DATA(cmptr));
#else
	if (msg.msg_accrightslen =! sizeof(int)) {
		_debug("Bad msg\n");
		if (_err_func)
			(*_err_func) ("Bad msg", ES_READ, ES_FD);
		return -1;
	}		

	/* Everything is good */
	*recvfd = newfd;
#endif
	
	_debug("Received \"%d\" : fd\n", *recvfd);
        return 0;
}

