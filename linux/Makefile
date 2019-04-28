BINDIR  := /usr/bin

all: xxtea server client

xxtea:
	$(MAKE) -C xxtea
server:
	$(MAKE) -C server
client:
	$(MAKE) -C client

.PHONY: all xxtea server client

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp server/octopus-server ${DESTDIR}${BINDIR}/
	cp server/octopus-devices ${DESTDIR}${BINDIR}/
	cp client/octopus-client ${DESTDIR}${BINDIR}/

clean:
	$(MAKE) -C xxtea clean
	$(MAKE) -C server clean
	$(MAKE) -C client clean
