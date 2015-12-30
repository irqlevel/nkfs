#pragma once

struct journal_tx {
	struct nkfs_sb *sb;
	u64 block;
};

int journal_init(struct nkfs_sb *sb);
void journal_deinit(struct nkfs_sb *sb);

int journal_tx_start(struct nkfs_sb *sb, struct journal_tx **ptx);
int journal_tx_write(struct nkfs_sb *sb, struct journal_tx *tx,
		     struct dio_cluster *clu);
int journal_tx_commit(struct nkfs_sb *sb, struct journal_tx *tx);
