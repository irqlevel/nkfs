#pragma once

#define DS_IMAGE_MAGIC 0x3EFFBDAE

#define DS_IMAGE_VER_1	1

struct ds_image_header {
	__be32			magic;
	__be32			version;
	__u64			size;
	struct ds_obj_id	id;
};

static inline __u32 ds_image_header_magic(struct ds_image_header *header)
{
	return be32_to_cpu(header->magic);
}

static inline __u32 ds_image_header_version(struct ds_image_header *header)
{
	return be32_to_cpu(header->version);
}

static inline __u64 ds_image_header_size(struct ds_image_header *header)
{
	return be64_to_cpu(header->size);
}

static inline void ds_image_header_id(struct ds_image_header *header, struct ds_obj_id *id)
{
	memcpy(id, &header->id, sizeof(*id));
}

static inline void ds_image_header_set_magic(struct ds_image_header *header, __u32 magic)
{
	header->magic = cpu_to_be32(magic);
}

static inline void ds_image_header_set_version(struct ds_image_header *header, __u32 version)
{
	header->version = cpu_to_be32(version);
}

static inline void ds_image_header_set_size(struct ds_image_header *header, __u64 size)
{
	header->size = cpu_to_be64(size);
}

static inline void ds_image_header_set_id(struct ds_image_header *header, struct ds_obj_id *id)
{
	memcpy(&header->id, id, sizeof(*id));
}

