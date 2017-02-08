# $FreeBSD$
#
# Compile netmap as a module, useful if you want a netmap bridge
# or loadable drivers.

.PATH: ${.CURDIR}/../../dev/regorus
.PATH.h: ${.CURDIR}/../../net
CFLAGS += -I${.CURDIR}/../../
KMOD	= regorus
SRCS	= regorus.c regorus.h

.include <bsd.kmod.mk>
