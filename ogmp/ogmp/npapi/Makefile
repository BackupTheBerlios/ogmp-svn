#!make
################################################################################
# Copyright (c) 1996 Netscape Communications. All rights reserved.
################################################################################
#
# Template plugin makefile
# This makefile contains some of our defines for the compiler:
#
# XP_UNIX	This needs to get defined for npapi.h on unix platforms.
# PLUGIN_TRACE	Enable this define to get debug prints whenever the plugin
#		api gets control.

CC= gcc
CXX= g++

GECKO_SDK=/home/hling/timedia-project/addon/gecko-sdk

CFLAGS= -Wall -g -O2  -I/usr/X11R6/include  -g -O2  -DMOZ_X11\
			 -I$(GECKO_SDK) -I$(GECKO_SDK)/include \
			 -Iinclude -fPIC -DMOZILLA_STRICT_API

LDFLAGS=    -L/usr/X11R6/lib  -lX11 -lSM -lICE -lXext -lX11 -lXpm -lXt \
				-L$(GECKO_SDK)/lib -lxpcomglue \
				-L$(GECKO_SDK)/bin -lnspr4 -lplds4

PLUGININFO= nsIScriptableOgmpPlugin.h \
				nsIScriptableOgmpPlugin.xpt

PLUGINIDL= nsIScriptableOgmpPlugin.idl

LIBOBJS=

OBJ= plugin.o nsScriptablePeer.o plugin-setup.o npp_gate.o \
		np_entry.o npn_gate.o strlcpy.o strlcat.o $(LIBOBJS)

INSTALL = /usr/bin/install -c
SHAREDTARGET=ogmpplugin.so

NP_HEADERS= include/npplat.h include/pluginbase.h

HEADERS=config.h $(NP_HEADERS)

SCRIPTABLEHEADERS=nsScriptablePeer.h plugin.h config.h \
	plugin-setup.h nsIScriptableOgmpPlugin.h $(HEADERS)

default all: $(SHAREDTARGET)

$(PLUGININFO): $(PLUGINIDL)
	$(GECKO_SDK)/bin/xpidl -I$(GECKO_SDK)/idl -m header $(PLUGINIDL)
	$(GECKO_SDK)/bin/xpidl -I$(GECKO_SDK)/idl -m typelib $(PLUGINIDL)

$(SHAREDTARGET): $(OBJ)
	$(CXX) -shared -o $(SHAREDTARGET) $(OBJ) $(CFLAGS) $(LDFLAGS)
	cp nsIScriptableOgmpPlugin.xpt ogmpplugin.xpt

np_entry.o: plugingate/np_entry.cpp
	$(CXX) -c $(CFLAGS) plugingate/np_entry.cpp

npp_gate.o: plugingate/npp_gate.cpp
	$(CXX) -c $(CFLAGS) plugingate/npp_gate.cpp

npn_gate.o: plugingate/npn_gate.cpp
	$(CXX) -c $(CFLAGS) plugingate/npn_gate.cpp

plugin.o: plugin.cpp $(SCRIPTABLEHEADERS)
	$(CXX) -c $(CFLAGS) $(LIBS) plugin.cpp

plugin-setup.o: plugin-setup.cpp $(SCRIPTABLEHEADERS)
	$(CXX) -c $(CFLAGS) $(LIBS) plugin-setup.cpp

nsScriptablePeer.o: nsScriptablePeer.cpp $(SCRIPTABLEHEADERS)
	$(CXX) -c $(CFLAGS) nsScriptablePeer.cpp
	
memmem.o: extras/memmem.c $(HEADERS)
	$(CXX) -c $(CFLAGS) extras/memmem.c

strlcat.o: extras/strlcat.c $(HEADERS)
	$(CXX) -c $(CFLAGS) extras/strlcat.c

strlcpy.o: extras/strlcpy.c $(HEADERS)
	$(CXX) -c $(CFLAGS) extras/strlcpy.c

clean:
	$(RM) $(OBJ) $(SHAREDTARGET) $(PLUGININFO)

install: $(SHAREDTARGET)
	if [ ! -d $(HOME)/.mozilla/plugins ]; then mkdir $(HOME)/.mozilla/plugins; fi
	$(INSTALL) $(SHAREDTARGET) $(HOME)/.mozilla/plugins
	$(INSTALL) nsIScriptableOgmpPlugin.xpt $(HOME)/.mozilla/plugins/ogmpplugin.xpt

uninstall:
	./uninstall.sh
	
sysinstall: $(SHAREDTARGET)
	./sysinstall.sh

