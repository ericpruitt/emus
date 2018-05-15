CC = c99
BASE_CFLAGS = -O1 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=500 -Wall $(CFLAGS)
BASE_LDFLAGS = -lcrypt -lpam -lX11 -lXext -lXrandr $(LDFLAGS)

SRC = screen-locker.c pam.c
OBJ = $(SRC:.c=.o)

all: screen-locker

.c.o:
	$(CC) -c $(BASE_CFLAGS) $<

screen-locker: $(OBJ)
	$(CC) -o $@ $(OBJ) $(BASE_LDFLAGS)

clean:
	rm -f screen-locker $(OBJ)
