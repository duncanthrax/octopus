src = $(wildcard *.c)
obj = $(src:.c=.o)

NAME    := octopus-server
CFLAGS   = -I/usr/include/libevdev-1.0 -I../xxtea -I. -L./jsmn -L../xxtea -DJSMN_STRICT=1
LDFLAGS  = -ljsmn -levdev -lxxtea -lbsd

.PHONY: all
all: jsmn $(NAME)
$(NAME): $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

jsmn: jsmn/jsmn.c jsmn/jsmn.o
	ar rc jsmn/libjsmn.a jsmn/jsmn.o

.PHONY: clean
clean:
	rm -f $(obj) $(NAME)
	rm -f jsmn/*.o jsmn/*.a

