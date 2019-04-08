SUBDIRS := server
BINDIR  := /usr/bin

all: server
server:
	$(MAKE) -C server

.PHONY: all server

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp server/octopus-server ${DESTDIR}${BINDIR}/
	cp server/octopus-devices ${DESTDIR}${BINDIR}/

clean:
	$(MAKE) -C server clean
