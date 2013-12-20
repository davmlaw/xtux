#======================================================#
# Makefile for XTux Arena CLIENT                       #
# (c) 1999-2000 David Lawrence - philaw@ozemail.com.au #
#======================================================#

#Where X is on your machine
X11_PATH = /usr/X11R6

#uncomment for Solaris (what is the OSTYPE variable?)
#SOCKLIB = -lsocket -lnsl


TOPLEVEL_PATH = ../..
COMMON_PATH = $(TOPLEVEL_PATH)/src/common
GGZ_PATH= $(TOPLEVEL_PATH)/src/ggz/client

# Programs used in building process
MAKE = make
CC = gcc

# Where to put the XTux client binary.
BINARY = $(TOPLEVEL_PATH)/xtux

# Flags used when compiling
CFLAGS = -g -Wall -O2 -DDATADIR=\"$(DATADIR)\"

# Paths to header files
X11_INC = -I$(X11_PATH)/include
COMMON_INC = -I$(COMMON_PATH)
GGZ_INC = -I$(GGZ_PATH)

# Libraries used in linking
X11LIB = -L$(X11_PATH)/lib -lX11
XPMLIB = -lXpm
MATHLIB = -lm
GGZLIB = $(GGZ_PATH)/libggz.a

# Common libraries
XTUXLIB = $(COMMON_PATH)/xtuxlib.a

# Objects to build
OBJECTS = main.o win.o draw.o input.o image.o menu.o misc.o cl_net.o entity.o intro.o cl_netmsg_recv.o cl_netmsg_send.o particle.o text_buffer.o

all:	client

clean:
	rm -f $(OBJECTS) $(BINARY)

#Apps
client: $(OBJECTS)
	$(CC) -o $(BINARY) $(CFLAGS) $(OBJECTS) $(XTUXLIB) $(MATHLIB) $(X11LIB) $(XPMLIB) $(SOCKLIB) $(GGZLIB)

# OBJECTS
.c.o:	client.h $*.c
	$(CC) $(CFLAGS) -c $*.c $(COMMON_INC) $(X11_INC) $(GGZ_INC)
