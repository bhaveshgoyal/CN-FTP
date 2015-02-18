CC = gcc
PROG = fsp
SRCS = main.c
all: $(PROG)
$(PROG):	$(SRCS)
	$(CC) -o $(PROG) $(SRCS)
