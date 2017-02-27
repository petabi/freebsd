/*
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/callout.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/vnet.h>
#include <net/netisr.h>
#include <net/regorus_internal.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

static void regorus_rx_handler(struct mbuf *);

static const uint8_t regorus_etheraddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }; /* TODO */
static const struct netisr_handler regorus_nh = {
	.nh_name = "regorus",
	.nh_handler = regorus_rx_handler,
	.nh_proto = NETISR_REGORUS,
	.nh_policy = NETISR_POLICY_SOURCE,
};

LIST_HEAD(, regorus_card) card_list;
static struct mtx card_mtx;
static struct regorus_task regorus_task;

static struct cdev *regorus_dev; /* /dev/regorus character device. */

static void regorus_transmit_probe(struct regorus_card *);
static void regorus_timer(void *);


static int regorus_card_create(const char *ifname);
static int regorus_task_init(struct regorus_task *task);
static void regorus_task_add(int kind, struct regorus_card *card, struct ifnet *ifp, struct regorushdr *pkt);


static int regorus_open(struct cdev *dev, int oflags, int devtype, struct thread *td);
static int regorus_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	int fflag, struct thread *td);
static int regorus_close(struct cdev *dev, int fflag, int devtype, struct thread *td);
struct cdevsw regorus_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "regorus",
	.d_open = regorus_open,
	.d_ioctl = regorus_ioctl,
	.d_close = regorus_close,
};

static int
regorus_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	int error = 0;

	(void)dev;
	(void)oflags;
	(void)devtype;
	(void)td;

	printf("regorus cdev open\n");
	/* TODO : do we need priv structure? */
	/*
	error = devfs_set_cdevpriv(priv, regorus_dtor);
	if (error) {
		free(priv, M_DEVBUF);
	}
	*/
	return error;
}

static int
regorus_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	int fflag, struct thread *td)
{
	int error = 0;
	struct regorus_req *req = (struct regorus_req *) data;
	struct regorus_card *card;
	int found = 0;

	switch (cmd) {
	case RIOCPROBE:
		/*
		 * Check if the requested interface
		 * already registered a regorus card.
		 * If not, start probing.
		 * User is expected to check the result later
		 */
		mtx_lock(&card_mtx);
		LIST_FOREACH(card, &card_list, card_list) {
			if (strncmp(card->ifname, req->ifname, IFNAMSIZ) == 0) {
				found = 1;
				break;
			}
		}
		req->found = found;
		if (found) {
			req->status = card->status;
		} else { /* register one & start probing */
			/*
			 * regorus_card_create will create a card strcture
			 * and insert it into card list
			 * and then start probing
			 */
			/* TODO : check return */
			regorus_card_create(req->ifname);
		}
		mtx_unlock(&card_mtx);
		break;
	case RIOCCONFIG:
		/* TODO */
		break;
	default:
		break;
	}
	return error;
}

static int
regorus_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	printf("regorus cdev closing\n");
	return 0;
}

static int regorus_init()
{
	int error = 0;
	/* Initialize data */
	mtx_init(&card_mtx, "regorus card list", NULL, MTX_DEF);
	LIST_INIT(&card_list);
	regorus_task_init(&regorus_task);

	regorus_dev = make_dev(&regorus_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "regorus");
	if (!regorus_dev) {
		error = EINVAL;
		goto fail;
	}

	/* RX handler */
	netisr_register(&regorus_nh);
	printf("regorus_modevent regorus_nh registered\n");
	return error;
fail:
	return error;
}

static int regorus_fini()
{
	// TODO : list clean-up?
	mtx_destroy(&card_mtx);
	return 0;
}

static int
regorus_modevent(module_t mod, int type, void *data)
{
	int error = 0;
	switch (type) {
	case MOD_LOAD:
		error = regorus_init();
		break;
	case MOD_UNLOAD:
		error = regorus_fini();
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (error);
}

static moduledata_t regorus_mod = {
	"regorus",
	regorus_modevent,
	0
};

DECLARE_MODULE(regorus, regorus_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
/*DEV_MODULE(regorus, regorus_modevent, NULL);*/
MODULE_VERSION(regorus, 1);

/*
 * Regorus rx side packet handler
 */
static void
regorus_rx_handler(struct mbuf *m)
{
	struct regorushdr *rr;
	struct regorushdr *regpkt;
	struct ifnet *ifp = m->m_pkthdr.rcvif;

	printf("REGORUS packet received on %s\n", if_name(ifp));
        if (m->m_len < sizeof(struct regorushdr) &&
	    ((m = m_pullup(m, sizeof(struct regorushdr))) == NULL)) {
		printf("REGORUS packet with short header received on %s\n",
		    if_name(ifp));
		return;
	}
	rr = mtod(m, struct regorushdr *);
	regpkt = malloc(sizeof(*regpkt), M_DEVBUF, M_NOWAIT|M_ZERO);
	memcpy(regpkt, rr, sizeof(*regpkt));

	switch(ntohs(rr->op)) {
	case REGORUS_PING:
		if_ref(ifp);
		regorus_task_add(REGORUS_WORK_PROBE_RX, NULL, ifp, regpkt);
		printf("REGORUS_PING received (info : 0x%x)\n", rr->info);
		break;
	case REGORUS_PONG:
		if_ref(ifp); /* TODO : ifnet list lock? */
		regorus_task_add(REGORUS_WORK_ACK_RX, NULL, ifp, regpkt);
		printf("REGORUS_PONG received (info : 0x%x)\n", rr->info);
		break;
	case REGORUS_REQ:
		printf("REGORUS_REQ received (info : 0x%x)\n", rr->info);
		break;
	case REGORUS_REP:
		printf("REGORUS_REP received (info : 0x%x)\n", rr->info);
		break;
	}
	m_freem(m);
}

/* TODO : aggregation? */

static void
regorus_transmit_ack(struct ifnet *ifp)
{
	struct regorushdr rh;
	struct ether_header *eh;
	struct mbuf *m;
	if (!ifp || (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) /* TODO */
		return;
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		return;

	eh = mtod(m, struct ether_header *);
	memcpy(eh->ether_shost, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	memcpy(eh->ether_dhost, regorus_etheraddr, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_REGORUS);

	rh.op = REGORUS_PONG;
	m->m_pkthdr.len = sizeof(*eh) + sizeof(rh);
	memcpy(mtod(m, caddr_t) + sizeof(*eh), &rh, sizeof(rh));

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len;
	ifp->if_transmit(ifp, m);
}

static void
regorus_transmit_probe(struct regorus_card *card)
{
	/* TODO : lock assert */
	struct ifnet *ifp = card->ifp;
	struct ether_header *eh;
	struct regorushdr rh;
	struct mbuf *m;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) /* TODO */
		return;

	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		return;

	eh = mtod(m, struct ether_header *);
	memcpy(eh->ether_shost, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	memcpy(eh->ether_dhost, regorus_etheraddr, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_REGORUS);

	rh.op = REGORUS_PING;
	m->m_pkthdr.len = sizeof(*eh) + sizeof(rh);
	memcpy(mtod(m, caddr_t) + sizeof(*eh), &rh, sizeof(rh));

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len;
	ifp->if_transmit(ifp, m);
}

static void regorus_timer(void *arg)
{
	/* TODO : cardmtx ownership assertion */
	struct regorus_card *card = (struct regorus_card *) arg;
	mtx_lock(&card_mtx);
	card->ref_count++; /* for reference in work : TODO atomic? */

	regorus_task_add(REGORUS_WORK_PROBE, card, NULL, NULL);

	card->ref_count--; /* my self : TODO atomic? */
	mtx_unlock(&card_mtx);
}

/* Regorus card handling */
static int regorus_card_create(const char *ifname)
{
	/* TODO : card_mtx should be asserted to be owned */
	int error = 0;
	struct ifnet *ifp = NULL;
	struct regorus_card *card = NULL;
	ifp = ifunit_ref(ifname);
	if (ifp == NULL) {
		error = ENXIO; /* TODO : check value */
	} else {
		/* Start setting up card related info to register
		 * and start probing by transmitting probing packet
		 */
		card = malloc(sizeof(*card), M_DEVBUF, M_WAITOK|M_ZERO);
		strcpy(card->ifname, ifname); /* TODO */
		card->ifp = ifp; /* We should unref this when cleaning up */
		card->status = REGORUS_CARD_DETECTING;
		card->ref_count++; /* TODO : atomic inc */
		card->retry_count = REGORUS_PROBE_RETRY;
		callout_init_mtx(&card->timer, &card_mtx, 0);
		LIST_INSERT_HEAD(&card_list, card, card_list);

		/* set up work and enqueue it and run the task */
		card->ref_count++; /* for reference in work */

		regorus_task_add(REGORUS_WORK_PROBE, card, NULL, NULL);
	}
	return error;
}

/* Regorus task related */

static int regorus_process_probe(struct regorus_card *card)
{
	/* TODO : be specific with return value */

	if (!card)
		return -1;

	mtx_lock(&card_mtx);
	if (card->status != REGORUS_CARD_DETECTING)
		return -1;

	if (card->retry_count <= 0) {
		card->retry_count = 0;
		return -1;
	}
	card->retry_count--;

	regorus_transmit_probe(card); /* TODO : specify type */

	card->ref_count++; // for timer TODO : atomic
	/* TODO : check if already running ? */
	callout_reset(&card->timer, hz, regorus_timer, card);
	mtx_unlock(&card_mtx);

	return 0;
}

static int regorus_process_ack_rx(struct ifnet *ifp, struct regorushdr *pkt)
{
	/* TODO : be specific with return value */
	struct regorus_card *card = NULL;
	int found = 0;

	if (!ifp)
		return -1;

	mtx_lock(&card_mtx);
	LIST_FOREACH(card, &card_list, card_list) {
		if (strncmp(card->ifname, ifp->if_xname, IFNAMSIZ) == 0) {
			found = 1;
			break;
		}
	}
	if (!found) { /* TODO */
		mtx_unlock(&card_mtx);
		return -1;
	}

	/* stop timer if any TODO */
	callout_stop(&card->timer);
	card->status = REGORUS_CARD_DETECTED;
	mtx_unlock(&card_mtx);
	return 0;
}

static int regorus_process_probe_rx(struct ifnet *ifp, struct regorushdr *pkt)
{
	/* TODO : be specific with return value */
	struct regorus_card *card = NULL;
	int found = 0;

	if (!ifp)
		return -1;

	regorus_transmit_ack(ifp);
	mtx_lock(&card_mtx);
	LIST_FOREACH(card, &card_list, card_list) {
		if (strncmp(card->ifname, ifp->if_xname, IFNAMSIZ) == 0) {
			found = 1;
			break;
		}
	}
	if (!found) { /* TODO */
		mtx_unlock(&card_mtx);
		return -1;
	}

	/* stop timer if any TODO */
	callout_stop(&card->timer);
	card->status = REGORUS_CARD_DETECTED;
	mtx_unlock(&card_mtx);
	return 0;
}


/* A func to be executed when a regorus task enqueued
 * (basically this serves as a dispatcher)
 */
static void regorus_process_work(void *arg, int pending __unused)
{
	struct regorus_task *task = (struct regorus_task *) arg;
	struct regorus_work *work = NULL;

	while (1) {
		/* get next work */
		mtx_lock(&task->queue_lock);
		work = STAILQ_FIRST(&task->head);
		if (work) {
			STAILQ_REMOVE_HEAD(&task->head, work_list);
		}
		mtx_unlock(&task->queue_lock);
		if (!work)
			break;

		/* process the work */
		switch (work->kind) {
			case REGORUS_WORK_PROBE:
				regorus_process_probe(work->card);
				break;
			case REGORUS_WORK_PROBE_RX:
				regorus_process_probe_rx(work->ifp, work->pkt);
				break;
			case REGORUS_WORK_ACK_RX:
				regorus_process_ack_rx(work->ifp, work->pkt);
				break;
			default:
				/* TODO */
				break;
		}

		/* clean-up the work */
		if (work->card) {
			mtx_lock(&card_mtx);
			work->card->ref_count--; // TODO : atomic
			mtx_unlock(&card_mtx);
		}
		if (work->ifp) {
			if_rele(work->ifp); /* TODO : ifnet list lock?? */
		}
		if (work->pkt) {
			free(work->pkt, M_DEVBUF);
		}
		free(work, M_DEVBUF);
	}
}

static int regorus_task_init(struct regorus_task *task)
{
	/* TODO */
	mtx_init(&task->queue_lock, "regorus task queue", NULL, MTX_DEF);
	STAILQ_INIT(&task->head);
	TASK_INIT(&task->task, 0, regorus_process_work, task);
	return 0;
}

static void regorus_task_add(int kind, struct regorus_card *card, struct ifnet *ifp, struct regorushdr *pkt)
{
	struct regorus_work *work;
	work = malloc(sizeof(*work), M_DEVBUF, M_NOWAIT|M_ZERO);
	work->kind = kind;
	work->card = card;
	work->ifp = ifp;
	work->pkt = pkt;

	mtx_lock(&regorus_task.queue_lock);
	STAILQ_INSERT_TAIL(&regorus_task.head, work, work_list);
	mtx_unlock(&regorus_task.queue_lock);
	taskqueue_enqueue(taskqueue_swi, &regorus_task.task);
}
