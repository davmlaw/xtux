#======================================================#
# Makefile for XTux Arena SERVER                       #
# (c) 1999-2000 David Lawrence - philaw@ozemail.com.au #
#======================================================#

#uncomment for Solaris (what is the OSTYPE variable?)
#SOCKLIB = -lsocket -lnsl

# PATHS
TOPLEVEL_PATH = ../..
COMMON_PATH = $(TOPLEVEL_PATH)/src/common
GGZ_PATH= $(TOPLEVEL_PATH)/src/ggz

# Programs used in building process
CC = gcc

# Where to put the XTux client binary.
BINARY = $(TOPLEVEL_PATH)/tux_serv

# Flags used when compiling
CFLAGS = -g -Wall -O2 -DDATADIR=\"$(DATADIR)\"

# Paths to header files
COMMON_INC = -I$(COMMON_PATH)
GGZ_INC = -I$(GGZ_PATH)/server -I$(GGZ_PATH)/easysock

# Libraries used in linking
MATHLIB = -lm

# Common libraries
XTUXLIB = $(COMMON_PATH)/xtuxlib.a
GGZLIB = $(GGZ_PATH)/server/libggzd.a $(GGZ_PATH)/easysock/libeasysock.a

# Objects to build
OBJECTS = main.o sv_net.o sv_netmsg_recv.o sv_netmsg_send.o world.o game.o entity.o weapon.o misc.o sv_map.o ai.o hitscan.o item.o event.o xtuxggz.o

all:	server

clean:
	rm -f $(OBJECTS) $(BINARY)

#Apps
server:	$(OBJECTS)
	$(CC) -o $(BINARY) $(OBJECTS) $(XTUXLIB) $(MATHLIB) $(SOCKLIB) $(GGZLIB)

# OBJECTS
.c.o:	server.h $*.h $*.c
	$(CC) $(CFLAGS) -c $*.c $(COMMON_INC) $(GGZ_INC)
