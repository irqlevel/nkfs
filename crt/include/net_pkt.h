#pragma once

void net_pkt_sum(struct ds_net_pkt *pkt, struct sha256_sum *sum);
int net_pkt_check(struct ds_net_pkt *pkt);
void net_pkt_sign(struct ds_net_pkt *pkt);
void net_pkt_zero(struct ds_net_pkt *pkt);
struct ds_net_pkt *net_pkt_alloc(void);
int net_pkt_check_dsum(struct ds_net_pkt *pkt, struct sha256_sum *dsum);
