#ifndef __NKFS_CLIENT_H__
#define __NKFS_CLIENT_H__

#include <include/nkfs_obj_id.h>
#include <include/nkfs_types.h>
#include <include/nkfs_obj_info.h>

#pragma pack(push, 1)
struct nkfs_con {
	int sock;
	int err;
};
#pragma pack(pop)

int  nkfs_connect(struct nkfs_con *con, char *ip, int port);
void nkfs_close(struct nkfs_con *con);

int  nkfs_put_object(struct nkfs_con *con, struct nkfs_obj_id *id, u64 off,
		     void *data, u32 data_size);
int  nkfs_get_object(struct nkfs_con *con,struct nkfs_obj_id *id, u64 off,
	void *data, u32 data_size, u32 *pread);

int  nkfs_delete_object(struct nkfs_con *con,struct nkfs_obj_id *id);
int  nkfs_create_object(struct nkfs_con *con,struct nkfs_obj_id *id);

int nkfs_echo(struct nkfs_con *con);

int nkfs_query_object(struct nkfs_con *con, struct nkfs_obj_id *id,
	struct nkfs_obj_info *info);

#endif
