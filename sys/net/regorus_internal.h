#ifndef _NET_REGORUS_INTERNAL_H_
#define _NET_REGORUS_INTERNAL_H_

#include <sys/callout.h>
#include <sys/queue.h>

#include <net/regorus.h>

/* Regorus packet header */
struct	regorushdr {
	u_short	op;		/* operation : one of: */
#define REGORUS_PING	1
#define REGORUS_PONG	2
#define REGORUS_REQ	3	/* configuration etc */
#define REGORUS_REP	4	/* result of config etc */
	u_short	dummy;		/* format of protocol address */
	uint32_t info;
};

/* Regorus card description */
struct regorus_card {
	LIST_ENTRY(regorus_card) card_list;
	char ifname[IFNAMSIZ];
	struct ifnet *ifp;	/* make this an interface */
        /*
         * status uses the following values
         * (defined in regorus.h)
         * REGORUS_CARD_DETECTING
         * REGORUS_CARD_DETECTED
         */
	int status;
	int retry_count; /* count to be used during probing */
#define REGORUS_PROBE_RETRY 3
	int ref_count; /* for resource management */
	struct callout timer; /* bridge callout */
	u_char mac[6];
};

/* Task structure for processing various Regorus related
 * job processing
 */
struct regorus_task {
	struct task task;
	struct mtx queue_lock; /* A lock used when enqueueing jobs */
	STAILQ_HEAD(work_head, regorus_work) head;
};

/* specific work to process */
struct regorus_work {
	STAILQ_ENTRY(regorus_work) work_list;
	int kind;
#define REGORUS_WORK_PROBE 1
#define REGORUS_WORK_PROBE_RX 2
#define REGORUS_WORK_ACK_RX 3
	struct regorus_card *card; /* could be null */
	struct ifnet *ifp;
	struct regorushdr *pkt;
	char ifname[IFNAMSIZ];
};
#endif /* !_NET_REGORUS_INTERNAL_H_ */
