#ifndef _NET_REGORUS_H_
#define _NET_REGORUS_H_

struct	regorushdr {
	u_short	op;		/* operation : one of: */
#define REGORUS_PING	1
#define REGORUS_PONG	2
#define REGORUS_REQ	3	/* configuration etc */
#define REGORUS_REP	4	/* result of config etc */
	u_short	dummy;		/* format of protocol address */
	uint32_t info;
};

#endif /* !_NET_REGORUS_H_ */
