#
# bootpd   - BOOTP server daemon
#
# $Header: Makefile,v 1.16 89/01/13 12:30:44 ww0n Exp $
#

# The next few lines may be uncommented and changed to alter the default
# filenames bootpd uses for its configuration and dump files.
#CONFFILE=-DCONFIG_FILE=\"/usr/etc/bootptab\"
#DUMPFILE=-DDUMP_FILE=\"/usr/etc/bootpd.dump\"
#FILESPECS=${CONFFILE} ${DUMPFILE}

# Users of SunOS 4.0 may have to add -DSUNOS40 to BOOTPDOPT below.
# Remove the -DVEND_CMU if you don't wish to support the "CMU vendor format"
# in addition to the RFC1048 format.

BOOTPDOPT=-DSYSLOG -DDEBUG -DVEND_CMU -DPRIVATE=static ${FILESPECS}

INSTALL=/usr/bin/install
DESTDIR=
ETCDIR=/etc
OBJS=bootpd.o readfile.o hash.o
CFLAGS=${BOOTPDOPT}

all: bootpd

bootpd:	${OBJS}
	sh newvers.sh Version.c
	${CC} ${CFLAGS} -o bootpd version.c ${OBJS}

bootpd.o: bootpd.c bootpd.h bootp.h
readfile.o: readfile.c bootpd.h bootp.h
hash.o: hash.c hash.h

system: install

install: all
	${INSTALL} -c -s bootpd ${DESTDIR}${ETCDIR}/bootpd

clean:
	@rm -f core bootpd *.BAK *.CKP *~ .emacs* *.o version.c
