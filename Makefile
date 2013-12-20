#============================================================#
# Top level Makefile for XTux Arena                          #
# David Lawrence (nogoodpunk@yahoo.com)                      #
# Makefile correction thanks to Milo van Handel (mwq@dds.nl) #
#============================================================#

#Change this to where you want to put the data directory.
DATADIR = `pwd`/data
#Eg you might want to put the data files here......
#DATADIR = /usr/share/games/xtux

MFLAGS = DATADIR=$(DATADIR)
CM_SRC_DIR = src/common
SV_SRC_DIR = src/server
CL_SRC_DIR = src/client
GZ_SRC_DIR = src/ggz

MAKE = @make -C

all:	common ggz client server

clean:
	$(MAKE) $(CM_SRC_DIR) clean
	$(MAKE) $(SV_SRC_DIR) clean
	$(MAKE) $(CL_SRC_DIR) clean
	$(MAKE) $(GZ_SRC_DIR) clean

common:
	@echo
	@echo "    ************* Building COMMON lib *************"
	@echo
	$(MAKE) $(CM_SRC_DIR) $(MFLAGS)
server:
	@echo
	@echo "    *************   Building SERVER   *************"
	@echo
	$(MAKE) $(SV_SRC_DIR) $(MFLAGS)

client:
	@echo
	@echo "    *************   Building CLIENT   *************"
	@echo
	$(MAKE) $(CL_SRC_DIR) $(MFLAGS)

ggz:
	@echo
	@echo "    *************   Building GGZ   *************"
	@echo
	$(MAKE) $(GZ_SRC_DIR) $(MFLAGS)

