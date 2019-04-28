src = $(wildcard *.c)
obj = $(src:.c=.o)

NAME    := octopus-client
CFLAGS   = -I/usr/include/libevdev-1.0 -I../xxtea -I. -L../xxtea
LDFLAGS  = -levdev -lxxtea

.PHONY: all
all: $(NAME)
$(NAME): $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(NAME)

