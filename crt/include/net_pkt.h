#ifndef __NKFS_CRT_NET_PKT_H__
#define __NKFS_CRT_NET_PKT_H__

void net_pkt_sum(struct nkfs_net_pkt *pkt, struct csum *sum);
int net_pkt_check(struct nkfs_net_pkt *pkt);
void net_pkt_sign(struct nkfs_net_pkt *pkt);
void net_pkt_zero(struct nkfs_net_pkt *pkt);
struct nkfs_net_pkt *net_pkt_alloc(void);
int net_pkt_check_dsum(struct nkfs_net_pkt *pkt, struct csum *dsum);

#endif
