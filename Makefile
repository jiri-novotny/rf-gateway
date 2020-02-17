CC=gcc
NAME=rfgw
LIBS=-lpthread -lcrypto
CFLAGS=-pedantic -Wall

all: compile remote

compile:
	$(CROSS_COMPILE)$(CC) *.c -o $(NAME) $(CFLAGS) $(LIBS)

remote:
	scp rfgw root@192.168.24.100:/tmp