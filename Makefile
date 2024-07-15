PROG = jsh

LIBFDR = /home/juliet/programming/libraries/libfdr

CC = gcc
INCLUDES = -I$(LIBFDR)
CFLAGS = -g -Wall -MD -std=gnu99 $(INCLUDES)

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
LIBS = $(LIBFDR)/libfdr.a

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -rf $(PROG) $(OBJ)

-include $(OBJ:.o=.d)
