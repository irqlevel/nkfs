#include "inc/nkfs_priv.h"

int journal_init(struct nkfs_sb *sb)
{
	return -ENOENT;
}

void journal_deinit(struct nkfs_sb *sb)
{
}

int journal_tx_start(struct nkfs_sb *sb, struct journal_tx **ptx)
{
	return -ENOENT;
}

int journal_tx_write(struct nkfs_sb *sb, struct journal_tx *tx,
		     struct dio_cluster *clu)
{
	return -ENOENT;
}

int journal_tx_commit(struct nkfs_sb *sb, struct journal_tx *tx)
{
	return -ENOENT;
}
