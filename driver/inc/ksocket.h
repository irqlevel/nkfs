#pragma once

#include <net/sock.h>

int ksock_create(struct socket **sockp,
	__u32 local_ip, int local_port);

int ksock_set_sendbufsize(struct socket *sock, int size);

int ksock_set_rcvbufsize(struct socket *sock, int size);

int ksock_connect(struct socket **sockp, __u32 local_ip, int local_port,
			__u32 peer_ip, int peer_port);

void ksock_release(struct socket *sock);

int ksock_write_timeout(struct socket *sock, void *buffer, int nob, int timeout, int *pwrote);

int ksock_read_timeout(struct socket *sock, void *buffer, int nob, int timeout, int *pread);

int ksock_listen(struct socket **sockp, __u32 local_ip, int local_port, int backlog);

int ksock_accept(struct socket **newsockp, struct socket *sock);

void ksock_abort_accept(struct socket *sock);


