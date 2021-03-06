/*
 * File: easysock.h
 * Author: Brent Hendricks
 * Project: libeasysock
 * Date: 4/16/98
 *
 * Declarations for my useful socket library
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


#ifndef _EASYSOCK_H
#define _EASYSOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/****************************************************************************
 * Version Numbers
 *
 * MAJOR.MINOR.PATCH
 *
 * Note: do not confuse this with the library soname
 *
 ***************************************************************************/
#define EASYSOCK_VERSION 0.2.0

/****************************************************************************
 * Error function
 *
 * Any error encountered in an easysock function will cause the error 
 * function to be called if one has been set.
 * 
 * Upon removal, the previous error function is returned
 *
 ***************************************************************************/
typedef enum {
	ES_CREATE,
	ES_READ,
	ES_WRITE,
	ES_ALLOCATE
} EsOpType;

typedef enum {
	ES_NONE,
	ES_CHAR,
	ES_INT,
	ES_STRING,
	ES_FD
} EsDataType;

typedef void (*es_err_func) (const char *, const EsOpType, 
			     const EsDataType);

int es_err_func_set(es_err_func func);
es_err_func es_err_func_rem(void);



/****************************************************************************
 * Exit function
 *
 * Any of the *_or_die() functions will call the set exit function
 * if there is an error.  If there is no set function, exit() will be 
 * called.
 * 
 * Upon removal, the old exit function is returned.
 *
 ***************************************************************************/
typedef void (*es_exit_func) (int);

int es_exit_func_set(es_exit_func func);
es_exit_func es_exit_func_rem(void);


/****************************************************************************
 * Creating a socket.
 * 
 * type  :  one of ES_SERVER or ES_CLIENT
 * port  :  tcp port number 
 * server:  hostname to connect to (only relevant for client)
 * 
 * Returns socket fd or -1 on error
 ***************************************************************************/
typedef enum {
	ES_SERVER,
	ES_CLIENT
} EsSockType;

int es_make_socket(const EsSockType type, const unsigned short port, 
		   const char *server);

int es_make_socket_or_die(const EsSockType type,
			  const unsigned short port, 
			  const char *server);

int es_make_unix_socket(const EsSockType type, const char* name);
int es_make_unix_socket_or_die(const EsSockType type, const char* name);


/****************************************************************************
 * Reading/Writing a single char.
 * 
 * sock  :  socket fd
 * data  :  single char for write.  pointer to char for read
 * 
 * Returns 0 if successful, -1 on error.
 ***************************************************************************/
int es_write_char(const int sock, const char data);
void es_write_char_or_die(const int sock, const char data);
int es_read_char(const int sock, char *data);
void es_read_char_or_die(const int sock, char *data);


/****************************************************************************
 * Reading/Writing an integer in network byte order
 * 
 * sock  :  socket fd
 * data  :  int for write.  pointer to int for read
 * 
 * Returns 0 if successful, -1 on error.
 ***************************************************************************/
int es_write_int(const int sock, const int data);
void es_write_int_or_die(const int sock, const int data);
int es_read_int(const int sock, int *data);
void es_read_int_or_die(const int sock, int *data);


/****************************************************************************
 * Reading/Writing a char string
 * 
 * sock  : socket fd
 * data  : char string or address of char string for alloc func
 * fmt   : format string for sprintf-like behavior  
 * len   : length of user-provided data buffer in bytes
 * 
 * Returns 0 if successful, -1 on error.
 *
 ***************************************************************************/
int es_write_string(const int sock, const char *data);
void es_write_string_or_die(const int sock, const char *data);
int es_va_write_string(const int sock, const char *fmt, ...);
void es_va_write_string_or_die(const int sock, const char *fmt, ...);
int es_read_string(const int sock, char *data, const unsigned int len);
void es_read_string_or_die(const int sock, char *data, const unsigned int len);
int es_read_string_alloc(const int sock, char **data);
void es_read_string_alloc_or_die(const int sock, char **data);


/****************************************************************************
 * Reading/Writing a file descriptor
 * 
 * sock  : socket fd
 * data  : address of data to be read/written
 * n     : size of data (in bytes) to be read/written
 * 
 * Returns 0 on success, or -1 on error
 *
 * Many thanks to Richard Stevens and his wonderful books, from which
 * these functions come.
 *
 ***************************************************************************/
int es_read_fd(const int sock, int *recvfd);
int es_write_fd(const int sock, int sendfd);

/****************************************************************************
 * Reading/Writing a byte sequence 
 * 
 * sock  : socket fd
 * data  : address of data to be read/written
 * n     : size of data (in bytes) to be read/written
 * 
 * Returns number of bytes processed, or -1 on error
 *
 * Many thanks to Richard Stevens and his wonderful books, from which
 * these functions come.
 *
 ***************************************************************************/
int es_readn(const int sock, void *data, size_t n);
int es_writen(const int sock, const void *vdata, size_t n);

#ifdef __cplusplus
}
#endif

#endif
