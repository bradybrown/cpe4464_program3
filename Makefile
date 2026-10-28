# Example makefile for CPE 464
# 

CC = gcc
CFLAGS = -g -Wall -Werror
OS = $(shell uname -s)
PROC = $(shell uname -p)
EXEC_SUFFIX=$(OS)-$(PROC)

ifeq ("$(OS)", "SunOS")
	OSLIB=-L/opt/csw/lib -R/opt/csw/lib -lsocket -lnsl
	OSINC=-I/opt/csw/include
	OSDEF=-DSOLARIS
else
ifeq ("$(OS)", "Darwin")
	OSLIB=
	OSINC=
	OSDEF=-DDARWIN
else
	OSLIB=
	OSINC=
	OSDEF=-DLINUX
endif
endif

all:  fishnode-$(EXEC_SUFFIX)


# TODO: Change files to this repo's names
fishnode-$(EXEC_SUFFIX): fishnode.c
	$(CC) $(CFLAGS) $(OSINC) $(OSLIB) $(OSDEF) -o $@ fishnode.c libfish-Linux-x86_64.a smartalloc.c
.PHONY: test

test:
	./run_tests.sh

GIVEN_FILES = fish.h libfish-Linux-x86_64.a smartalloc.c smartalloc.h fishnode.h
handin: README
	~bellardo/bin/rcvhandin bellardo p2 fishnode.c README Makefile $(GIVEN_FILES)

clean:
	rm -rf fishnode-* fishnode-*.dSYM
