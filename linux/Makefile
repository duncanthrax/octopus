BINDIR  := /usr/bin

all: xxtea server

xxtea:
	$(MAKE) -C xxtea
server:
	$(MAKE) -C server

.PHONY: all xxtea server

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp server/octopus-server ${DESTDIR}${BINDIR}/
	cp server/octopus-devices ${DESTDIR}${BINDIR}/

clean:
	$(MAKE) -C xxtea clean
	$(MAKE) -C server clean
