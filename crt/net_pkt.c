#include <crt/include/crt.h>

void net_pkt_sum(struct ds_net_pkt *pkt, struct csum *sum)
{
	struct csum_ctx ctx;

	csum_reset(&ctx);
	csum_update(&ctx, pkt, offsetof(struct ds_net_pkt, sum));
	csum_digest(&ctx, sum);
}
EXPORT_SYMBOL(net_pkt_sum);

int net_pkt_check(struct ds_net_pkt *pkt)
{
	struct csum sum;
	if (pkt->dsize && pkt->dsize > DS_NET_PKT_MAX_DSIZE) {
		CLOG(CL_ERR, "invalid pkt dsize %llu", pkt->dsize);
		return -EINVAL;
	}

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

int net_pkt_check_dsum(struct ds_net_pkt *pkt, struct csum *dsum)
{
	if (0 != crt_memcmp(&pkt->dsum, dsum, sizeof(*dsum))) {
		CLOG(CL_ERR, "invalid pkt dsum");
		return -EINVAL;	
	}
	return 0;
}
EXPORT_SYMBOL(net_pkt_check_dsum);
