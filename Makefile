CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11 -O2

SRC := src/webability_api.c
OBJ := $(SRC:.c=.o)
LIB := libwebability_api.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB)
