#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ixgbe

KMOD    = if_ixgbe
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += ixgbe.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP

.if defined(DEV_NETMAP)
CFLAGS+= -DDEV_NETMAP
.endif

.include <bsd.kmod.mk>
