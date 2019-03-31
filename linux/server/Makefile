src = $(wildcard *.c)
obj = $(src:.c=.o)

NAME    := octopus-server
BINDIR  := /usr/bin
UDEVDIR := /etc/udev/rules.d
CFLAGS   = -I/usr/include/libevdev-1.0 -I. -L./jsmn
LDFLAGS  = -ljsmn -levdev

.PHONY: all
all: $(NAME)
$(NAME): $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp $(NAME) ${DESTDIR}${BINDIR}/
	mkdir -p ${DESTDIR}${UDEVDIR}
	cp *.rules ${DESTDIR}${UDEVDIR}/

.PHONY: clean
clean:
	rm -f $(obj) $(NAME)
