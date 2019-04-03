CC=gcc
NAME=rfgw
LIBS=-lpthread -lcrypto
CFLAGS=-pedantic -Wall

all:
	$(CROSS_COMPILE)$(CC) *.c -o $(NAME) $(CFLAGS) $(LIBS)
