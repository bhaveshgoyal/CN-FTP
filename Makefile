CC = gcc
PROG = fsp
SRCS = main.c
LIBS = -lpthread -lssl -lcrypto -g -Wall
all: $(PROG)
$(PROG):	$(SRCS)
	$(CC) -o $(PROG) $(SRCS) $(LIBS)

clean:
	rm -f $(PROG)
