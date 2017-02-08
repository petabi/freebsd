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
#include <net/regorus.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

static void	regorus_handler(struct mbuf *);

static const struct netisr_handler regorus_nh = {
	.nh_name = "regorus",
	.nh_handler = regorus_handler,
	.nh_proto = NETISR_REGORUS,
	.nh_policy = NETISR_POLICY_SOURCE,
};

static int
regorus_modevent(module_t mod, int type, void *data)
{
	int error = 0;
	switch (type) {
	case MOD_LOAD:
		/*TODO error = regorus_init();*/
		netisr_register(&regorus_nh);
		printf("regorus_modevent regorus_nh registered\n");
		break;
	case MOD_UNLOAD:
		/* TODO : regorus_fini() */
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
