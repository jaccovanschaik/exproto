# Makefile: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: Makefile 13 2013-10-23 18:48:56Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

EXPROTO = exproto.o

JVS_TOP = $(HOME)
JVS_LIB = -L$(JVS_TOP)/lib -ljvs
JVS_INC = -I$(JVS_TOP)/include

INSTALL_TOP = $(HOME)
INSTALL_BIN = $(INSTALL_TOP)/bin

CFLAGS = -g $(JVS_INC)

exproto: $(EXPROTO)
	$(CC) $(CFLAGS) -o exproto $(EXPROTO) $(JVS_LIB) -lm

clean:
	rm -f *.o exproto core exproto.tgz

exproto.tgz: clean
	tar cvf - `ls | grep -v exproto.tgz` | gzip > exproto.tgz

install: exproto
	if [ ! -d $(INSTALL_BIN) ]; then mkdir -p $(INSTALL_BIN); fi
	cp exproto $(INSTALL_BIN)

# Dependencies - generated by deps on Sat Oct 19 21:51:42 CEST 2013

exproto.o: exproto.c 