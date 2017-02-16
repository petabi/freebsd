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

static void regorus_handler(struct mbuf *);

static const uint8_t regorus_etheraddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }; /* TODO */
static const struct netisr_handler regorus_nh = {
	.nh_name = "regorus",
	.nh_handler = regorus_handler,
	.nh_proto = NETISR_REGORUS,
	.nh_policy = NETISR_POLICY_SOURCE,
};

LIST_HEAD(, regorus_card) card_list;
static struct mtx card_list_mtx;
static struct mtx cdev_mtx;

static struct cdev *regorus_dev; /* /dev/regorus character device. */

static void regorus_transmit(struct regorus_card *);
static void regorus_timer(void *);

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
		mtx_lock(&card_list_mtx);
		LIST_FOREACH(card, &card_list, card_list) {
			if (strcmp(card->ifname, req->ifname) == 0) {
				found = 1;
				break;
			}
		}
		req->found = found;
		if (found) {
			req->status = card->status;
		} else { /* register one & start probing */
			/* First check if the requested interface exists */
			struct ifnet *ifp = NULL;
			ifp = ifunit_ref(req->ifname);
			if (ifp == NULL) {
				error = ENXIO; /* TODO : check value */
			} else {
				/* Start setting up card related info to register
				 * and start probing by transmitting probing packet
				 */
				card = malloc(sizeof(*card), M_DEVBUF, M_WAITOK|M_ZERO);
				mtx_init(&card->lock, "regorus_card", NULL, MTX_DEF);
				callout_init_mtx(&card->timer, &card->lock, 0);

				mtx_lock(&card->lock);
				card->ifp = ifp;
				regorus_transmit(card); /* TODO : specify type */
				mtx_unlock(&card->lock);
				callout_reset(&card->timer, hz, regorus_timer, card);

				LIST_INSERT_HEAD(&card_list, card, card_list);
				req->status = REGORUS_CARD_DETECTING;
			}
		}
		mtx_unlock(&card_list_mtx);
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
	mtx_init(&card_list_mtx, "regorus card list", NULL, MTX_DEF);
	LIST_INIT(&card_list);
	mtx_init(&cdev_mtx, "regorus cdev", NULL, MTX_DEF);

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
	mtx_destroy(&card_list_mtx);
	mtx_destroy(&cdev_mtx);
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
regorus_handler(struct mbuf *m)
{
	struct regorushdr *rr;
	struct ifnet *ifp = m->m_pkthdr.rcvif;

	printf("REGORUS packet received on %s\n", if_name(ifp));
        if (m->m_len < sizeof(struct regorushdr) &&
	    ((m = m_pullup(m, sizeof(struct regorushdr))) == NULL)) {
		printf("REGORUS packet with short header received on %s\n",
		    if_name(ifp));
		return;
	}
	rr = mtod(m, struct regorushdr *);

	switch(ntohs(rr->op)) {
	case REGORUS_PING:
		printf("REGORUS_PING received (info : 0x%x)\n", rr->info);
		break;
	case REGORUS_PONG:
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

/* TODO : specify type */

static void
regorus_transmit(struct regorus_card *card)
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

	/* TODO : generality */
	rh.op = REGORUS_PING;
	m->m_pkthdr.len = sizeof(*eh) + sizeof(rh);
	memcpy(mtod(m, caddr_t) + sizeof(*eh), &rh, sizeof(rh));

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len;
	ifp->if_transmit(ifp, m);
}

static void	regorus_timer(void *arg)
{
}
