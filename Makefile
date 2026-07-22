CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11 -O2

SRC := src/webability_api.c src/json.c src/dns.c src/mail.c
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

# Esta regla solo empaqueta libwebability_api.a — no enlaza nada.
# El consumidor final (tu programa) debe enlazar con -lcurl -lcrypto además
# de esta librería, por ejemplo:
#   cc myapp.c -Lpath/a/webability-c -lwebability_api -lcurl -lcrypto -o myapp
