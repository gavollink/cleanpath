SOURCE=bstr.c cleanpath.c
DEPEND=Makefile cleanpath.c $(SOURCE)
CC=gcc
CCFLAGS=-Wall -O2
CCFLAGS+=-std=gnu99
#CCFLAGS+=-ggdb
#CCFLAGS+=-DDEBUG

cleanpath: $(DEPEND)
	$(CC) $(CCFLAGS) -o cleanpath $(SOURCE)
