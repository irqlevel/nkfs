#undef TRACE_SYSTEM
#define TRACE_SYSTEM nkfs

#if !defined(_NKFS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _NKFS_TRACE_H

#include <linux/tracepoint.h>

#include "dio.h"
#include "inode.h"

DECLARE_EVENT_CLASS(balloc_block_alloc_class,
	TP_PROTO(u64 block),
	TP_ARGS(block),
	TP_STRUCT__entry(
		__field(u64, block)
	),
	TP_fast_assign(
		__entry->block = block
	),
	TP_printk("blk = %llu", __entry->block)
);

#define DEFINE_BLOCK_EVENT(name)		\
DEFINE_EVENT(balloc_block_alloc_class, name,	\
	TP_PROTO(u64 block),			\
	TP_ARGS(block))

DEFINE_BLOCK_EVENT(balloc_block_alloc);
DEFINE_BLOCK_EVENT(balloc_block_free);

DECLARE_EVENT_CLASS(inode_class,
	TP_PROTO(struct nkfs_inode *inode),
	TP_ARGS(inode),
	TP_STRUCT__entry(
		__field(u64, block)
		__field(u64, blocks_tree)
		__field(u64, blocks_sum_tree)
	),
	TP_fast_assign(
		__entry->block = inode->block;
		__entry->blocks_tree = inode->blocks_tree_block;
		__entry->blocks_sum_tree = inode->blocks_sum_tree_block;
	),
	TP_printk("blk = %llu, blk_t = %llu, blk_sum_t = %llu",
		__entry->block, __entry->blocks_tree, __entry->blocks_sum_tree)
);

#define DEFINE_TREE_EVENT(name)			\
DEFINE_EVENT(inode_class, name,			\
	TP_PROTO(struct nkfs_inode *inode),	\
	TP_ARGS(inode))

DEFINE_TREE_EVENT(inode_create);

DECLARE_EVENT_CLASS(inode_write_block_class,
	TP_PROTO(struct nkfs_inode *inode, struct inode_block *ib),
	TP_ARGS(inode, ib),
	TP_STRUCT__entry(
		__field(u64, block)
		__field(u64, clu_index)
		__field(u64, sum_clu_index)
	),
	TP_fast_assign(
		__entry->block = inode->block;
		__entry->clu_index = ib->clu->index;
		__entry->sum_clu_index = ib->sum_clu->index;
	),
	TP_printk("iblk = %llu, clu = %llu, sclu = %llu",
		__entry->block, __entry->clu_index, __entry->sum_clu_index)
);

#define DEFINE_WRITE_EVENT(name)					\
DEFINE_EVENT(inode_write_block_class, name,				\
	TP_PROTO(struct nkfs_inode *inode, struct inode_block *ib),	\
	TP_ARGS(inode, ib))

DEFINE_WRITE_EVENT(inode_write_block);

DECLARE_EVENT_CLASS(dio_clu_class,
	TP_PROTO(struct dio_cluster *cluster),
	TP_ARGS(cluster),
	TP_STRUCT__entry(
		__field(u64, index)
	),
	TP_fast_assign(
		__entry->index = cluster->index;
	),
	TP_printk("i = %llu", __entry->index)
);

#define DEFINE_DIO_EVENT(name)				\
DEFINE_EVENT(dio_clu_class, name,			\
	TP_PROTO(struct dio_cluster *cluster),		\
	TP_ARGS(cluster))

DEFINE_DIO_EVENT(dio_clu_sync);

TRACE_EVENT(dio_submit,
	TP_PROTO(struct dio_io *io),
	TP_ARGS(io),

	TP_STRUCT__entry(
		__field(void *, io)
		__field(void *, bio)
		__field(unsigned long, rw)
		__field(u64, sector)
		__field(u32, size)
	),

	TP_fast_assign(
		__entry->io = io;
		__entry->rw = io->rw;
		__entry->bio = io->bio;
		__entry->sector = BIO_BI_SECTOR(io->bio);
		__entry->size = BIO_BI_SIZE(io->bio);
	),

	TP_printk("io %p rw 0x%lx bio %p sector %llu size %u",
		  __entry->io, __entry->rw, __entry->bio, __entry->sector,
		  __entry->size)
);

TRACE_EVENT(dio_io_end_bio,
	TP_PROTO(struct dio_io *io),
	TP_ARGS(io),

	TP_STRUCT__entry(
		__field(void *, io)
		__field(void *, bio)
		__field(unsigned long, rw)
		__field(u64, sector)
		__field(u32, size)
		__field(int, err)
	),

	TP_fast_assign(
		__entry->io = io;
		__entry->rw = io->rw;
		__entry->bio = io->bio;
		__entry->sector = BIO_BI_SECTOR(io->bio);
		__entry->size = BIO_BI_SIZE(io->bio);
		__entry->err = io->err;
	),

	TP_printk("io %p err %d rw 0x%lx bio %p sector %llu size %u",
		  __entry->io, __entry->err, __entry->rw, __entry->bio,
		  __entry->sector, __entry->size)
);

#endif /* _NKFS_TRACE_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../core
#define TRACE_INCLUDE_FILE trace_events
#include <trace/define_trace.h>
