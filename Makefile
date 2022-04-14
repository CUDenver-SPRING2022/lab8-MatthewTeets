# Makefile for lab8 server/client program #

CC = gcc
OBJCSS = server_client8.c

CFLAGS = -g -Wall -lm

#setup for system
nLIBS = 

all: client8

client8: $(OBJCSS)
	$(CC) $(CFLAGS) -o $@ $(OBJCSS) $(LIBS)

clean:
	rm client8
