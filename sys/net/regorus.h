#ifndef _NET_REGORUS_H_
#define _NET_REGORUS_H_

#include <sys/callout.h>
#include <sys/queue.h>

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
#define REGORUS_CARD_DETECTING	0
#define REGORUS_CARD_DETECTED	1
	int status;
	struct callout		timer;	/* bridge callout */
	u_char mac[6];
};

/*
 * Provided to user
 * (TODO : separation?)
 */
struct regorus_req {
	char ifname[IFNAMSIZ];
	uint16_t regorus_cmd;
	int found;
	int status;
};

#define RIOCPROBE	_IOWR('i', 1, struct regorus_req)
#define RIOCCONFIG	_IOWR('i', 2, struct regorus_req)

#endif /* !_NET_REGORUS_H_ */
