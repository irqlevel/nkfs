#include <crt/include/crt.h>

void net_pkt_sum(struct ds_net_pkt *pkt, struct sha256_sum *sum)
{
	sha256(pkt, offsetof(struct ds_net_pkt, sum),
			sum, 0);
}
EXPORT_SYMBOL(net_pkt_sum);

int net_pkt_check(struct ds_net_pkt *pkt)
{
	struct sha256_sum sum;
	if (pkt->sign1 != DS_NET_PKT_SIGN1 ||
			pkt->sign2 != DS_NET_PKT_SIGN2) {
		CLOG(CL_ERR, "invalid pkt signs");
		return -EINVAL;
	}

	net_pkt_sum(pkt, &sum);
	if (0 != crt_memcmp(&pkt->sum, &sum, sizeof(sum))) {
		CLOG(CL_ERR, "invalid pkt sum");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(net_pkt_check);

void net_pkt_sign(struct ds_net_pkt *pkt)
{
	pkt->sign1 = DS_NET_PKT_SIGN1;
	pkt->sign2 = DS_NET_PKT_SIGN2;
	net_pkt_sum(pkt, &pkt->sum);
}
EXPORT_SYMBOL(net_pkt_sign);

void net_pkt_zero(struct ds_net_pkt *pkt)
{
	crt_memset(pkt, 0, sizeof(*pkt));
}
EXPORT_SYMBOL(net_pkt_zero);

struct ds_net_pkt *net_pkt_alloc(void)
{
	struct ds_net_pkt *pkt;
	pkt = crt_malloc(sizeof(struct ds_net_pkt));
	if (!pkt) {
		CLOG(CL_ERR, "no memory");
		return NULL;
	}
	net_pkt_zero(pkt);
	return pkt;
}
EXPORT_SYMBOL(net_pkt_alloc);

int net_pkt_check_dsum(struct ds_net_pkt *pkt, struct sha256_sum *dsum)
{
	if (0 != crt_memcmp(&pkt->dsum, dsum, sizeof(*dsum))) {
		CLOG(CL_ERR, "invalid pkt dsum");
		return -EINVAL;	
	}
	return 0;
}
EXPORT_SYMBOL(net_pkt_check_dsum);
