#ifndef _NET_REGORUS_H_
#define _NET_REGORUS_H_

#include <net/if.h>

#define REGORUS_CARD_DETECTING	0
#define REGORUS_CARD_DETECTED	1

struct regorus_req {
	char ifname[IFNAMSIZ];
	uint16_t regorus_cmd;
	int found;
	int status;
};

#define RIOCPROBE	_IOWR('i', 1, struct regorus_req)
#define RIOCCONFIG	_IOWR('i', 2, struct regorus_req)

#endif /* !_NET_REGORUS_H_ */
