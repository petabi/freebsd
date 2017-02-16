#ifndef _NET_REGORUS_INTERNAL_H_
#define _NET_REGORUS_INTERNAL_H_

#include <sys/callout.h>
#include <sys/queue.h>

#include <net/regorus.h>

struct	regorushdr {
	u_short	op;		/* operation : one of: */
#define REGORUS_PING	1
#define REGORUS_PONG	2
#define REGORUS_REQ	3	/* configuration etc */
#define REGORUS_REP	4	/* result of config etc */
	u_short	dummy;		/* format of protocol address */
	uint32_t info;
};

struct regorus_card {
	LIST_ENTRY(regorus_card)	card_list; /* neighbor */
	struct mtx		lock;
	char ifname[IFNAMSIZ];
	struct ifnet		*ifp;	/* make this an interface */
        /*
         * status uses the following values
         * (defined in regorus.h)
         * REGORUS_CARD_DETECTING
         * REGORUS_CARD_DETECTED
         */
	int status;
	struct callout		timer;	/* bridge callout */
	u_char mac[6];
};

#endif /* !_NET_REGORUS_INTERNAL_H_ */
