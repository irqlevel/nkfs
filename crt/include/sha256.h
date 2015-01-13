#pragma once

#pragma pack(push, 1)
struct sha256_sum {
	unsigned char bytes[32];	
};
#pragma pack(pop)

/**
 * \brief          SHA-256 context structure
 */
struct sha256_context
{
    u32 total[2];	         /*!< number of bytes processed  */
    u32 state[8];       	   /*!< intermediate digest state  */
    unsigned char buffer[64];   /*!< data block being processed */

    unsigned char ipad[64];     /*!< HMAC: inner padding        */
    unsigned char opad[64];     /*!< HMAC: outer padding        */
    int is224;                  /*!< 0 => SHA-256, else SHA-224 */
};

/**
 * \brief          Output = SHA-256( input buffer )
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   SHA-224/256 checksum result
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
extern void sha256( void *input, size_t ilen,
           struct sha256_sum *output, int is224 );

/**
 * \brief          Initialize SHA-256 context
 *
 * \param ctx      SHA-256 context to be initialized
 */
extern void sha256_init( struct sha256_context *ctx );

/**
 * \brief          Clear SHA-256 context
 *
 * \param ctx      SHA-256 context to be cleared
 */
extern void sha256_free( struct sha256_context *ctx );

/**
 * \brief          SHA-256 context setup
 *
 * \param ctx      context to be initialized
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
extern void sha256_starts( struct sha256_context *ctx, int is224 );

/**
 * \brief          SHA-256 process buffer
 *
 * \param ctx      SHA-256 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
extern void sha256_update( struct sha256_context *ctx, void *ibuf,
                    size_t ilen );

/**
 * \brief          SHA-256 final digest
 *
 * \param ctx      SHA-256 context
 * \param output   SHA-224/256 checksum result
 */
extern void sha256_finish(struct sha256_context *ctx, struct sha256_sum *output);

extern void __sha256_test(void);

extern char *sha256_sum_hex(struct sha256_sum *sum);
