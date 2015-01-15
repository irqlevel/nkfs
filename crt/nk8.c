#include <crt/include/crt.h>

static int inited = 0;
static u8 gf_log[0x100][0x100];
static u8 gf_alog[0x100][0x100];

#define gf_mult(a, b)	gf_log[(a)][(b)]
#define gf_div(a, b)	gf_alog[(a)][(b)]

static void gf_mult_matrix(u8** matrix1, u8** matrix2,
		u8** result, u32 size);

#define MAX_K 50
#define MAX_N 200
#define MIN_N 2
#define MIN_K 2

int gf_tables_save(char *fpath)
{
	void *file;
	loff_t off;
	int err;

	file = crt_file_open(fpath);
	if (!file) {
		CLOG(CL_ERR, "cant open file %s", fpath);
		return -EIO;
	}
	err = crt_file_write(file, gf_log, sizeof(gf_log), &off);
	if (err) {
		CLOG(CL_ERR, "cant write to file %s err %d",
			fpath, err);
		goto close;
	}

	err = crt_file_write(file, gf_alog, sizeof(gf_alog), &off);
	if (err) {
		CLOG(CL_ERR, "cant write to file %s err %d",
			fpath, err);
		goto close;
	}
	err = crt_file_sync(file);
	if (err) {
		CLOG(CL_ERR, "cant sync file %s err %d",
			fpath, err);
		goto close;
	}
	err = 0;
close:
	crt_file_close(file);
	return err;
}

static u8 gf_mult_direct(u8 m, u8 n)
{
	unsigned long k = 0;
	int i;

	if (m == 0 || n == 0)
		return 0;

	for (i = 7; i >= 0; i--)
		if (m >> i) {
			k ^= (n << i);
			m ^= (1 << i);
		}

	if (k >> 8)
		for (i = 15; i >= 8; i--)
			if ( k >> i )
				k ^= (283 << (i-8));

	return	k;
}

static u8 gf_div_brute(u8 i, u8 j) 
{	
	int found = 0;
	int k;

	if (i == 0 || j == 0)
		return 0;

	for (k = 1; k < 0x100; k++) {
    		if (gf_mult(k, j) == i) {
      			found = 1;
      			break;
    		}
	}

	return (found) ? k : 0;
}

static void gf_init(void)
{
	int i, j;
	u32 count = 0;

	for (i = 0; i < 0x100; i++) {
		for (j = 0; j < 0x100; j++) {
			gf_log[i][j] = gf_mult_direct(i, j);
			count++;
			if ((count % 1000) == 0) {
				CLOG(CL_DBG, "count %u i %u j %u",
					count, i, j);
				crt_msleep(1);
			}
		}
	}

	count = 0;
	for (i = 0; i < 0x100; i++) {
		for (j = 0; j < 0x100; j++) {
			gf_alog[i][j] = gf_div_brute(i, j);
			count++;
			if ((count % 1000) == 0) {
				CLOG(CL_DBG, "count %u i %u j %u",
					count, i, j);
				crt_msleep(1);
			}
		}
	}
}

static void gf_mult_matrix(u8** matrix1, u8** matrix2,
		u8** result, u32 size)
{
	int i = 0, j = 0, k = 0;
	for (i = 0; i < size; i++) {
		for (j = 0; j < size; j++) {
			result[i][j] = 0;
			for (k = 0; k < size; k++) {
				result[i][j] = (gf_mult(matrix1[i][k],
					matrix2[k][j])) ^ (result[i][j]);
			}
		}
	}
}

static void gf_swap(u8 a, u8 b)
{
	u8 tmp;
	tmp = a;
	a = b;
	b = tmp;
}

static void gf_rnd_matrix(u8 **matrix, u32 size)
{
	int i, j;

	for (i = 0; i < size; i++) {
		for (j = 0; j < size; j++) {
			matrix[i][j] = rand_u32_up(256);
		}
	}
}

static int gf_alloc_matrix(u8 ***pmatrix, u32 size)
{
	u8 **matrix;
	int i, j;
	int err;

	matrix = crt_malloc(size*sizeof(u8 *));
	if (!matrix) {
		CLOG(CL_ERR, "no memory");
		return -ENOMEM;
	}
	for (i = 0; i < size; i++) {
		matrix[i] = crt_malloc(size*sizeof(u8));
		if (!matrix[i]) {
			CLOG(CL_ERR, "no memory");
			err = -ENOMEM;
			goto fail;	
		}
		crt_memset(matrix[i], 0, size*sizeof(u8));	
	}

	*pmatrix = matrix;
	return 0;
fail:
	for (j = 0; j < i; j++)
		crt_free(matrix[j]);
	crt_free(matrix);
	return err;
}

static void gf_free_matrix(u8 **matrix, u32 size)
{
	int i;
	for (i = 0; i < size; i++) {
		crt_free(matrix[i]);
	}
	crt_free(matrix);
}

static int gf_inverse_matrix(u8 **matrix, u8**inverse, u32 size)
{
	int i, j, k, m, tmp;
	int ok;

	for (i = 0; i < size; i++) {
		for (j = 0; j < size; j++) {
			if (i != j) {
				inverse[i][j] = 0;
			} else {
				inverse[i][j] = 1;
			}
		}
	}

	for (i = 0; i < size; i++) {
		ok = 0;
		for (j = i; j < size; j++) {
      			if (matrix[i][j] > 0) {
				ok = 1;
				if (i != j) {
					for (k = 0; k < size; k++) {
						gf_swap(matrix[i][k], matrix[j][k]);
						gf_swap(inverse[i][k], inverse[j][k]);
					}
				}
 
				for (k = i+1; k < size; k++) {
					tmp = gf_div(matrix[k][i], matrix[i][i]);

					for (m = 0; m < size; m++) {
						if (m >= i) {
							(matrix[k][m]) ^=
								(gf_mult(matrix[i][m], tmp));
						}
						(inverse[k][m]) ^=
							(gf_mult(inverse[i][m], tmp));
					}
				}
				break;
			}
		}
		if (!ok) {
			CLOG(CL_ERR, "cant make inverse");
			return -EFAULT;
		}
	}

	for (i = size - 1; i > 0; i--) {
		for (j = i-1; j >= 0; j--) {
			tmp = gf_div(matrix[j][i], matrix[i][i]);
			matrix[j][i]=0;
			for (k = 0; k < size; k++) {
				inverse[j][k] ^= (gf_mult(tmp, inverse[i][k]));
			}
		}
 		for (k = 0; k < size; k++) {
			inverse[i][k] = gf_div(inverse[i][k], matrix[i][i]);
		}
	}

	for (k = 0; k < size; k++) {
		inverse[0][k] = gf_div(inverse[0][k], matrix[0][0]);
 	}

  	return 0;
}

static int gf_test_matrix(void)
{
	u8 **matrix, **inverse, **mult;
	u32 size = 10;
	int err;

	err = gf_alloc_matrix(&matrix, size);
	if (err) {
		CLOG(CL_ERR, "cant alloc matrix err %d", err);
		goto out;
	}	
	gf_rnd_matrix(matrix, size);

	err = gf_alloc_matrix(&inverse, size);
	if (err) {
		CLOG(CL_ERR, "cant alloc matrix err %d", err);
		goto free_matrix;
	}	

	err = gf_alloc_matrix(&mult, size);
	if (err) {
		CLOG(CL_ERR, "cant alloc matrix err %d", err);
		goto free_inverse;
	}

	gf_mult_matrix(matrix, matrix, mult, size);

	err = gf_inverse_matrix(mult, inverse, size);
	if (err) {
		CLOG(CL_ERR, "cant inverse err %d", err);
		goto free_mult;
	}
	CLOG(CL_INF, "complete");
free_mult:
	gf_free_matrix(mult, size);
free_inverse:
	gf_free_matrix(inverse, size);
free_matrix:
	gf_free_matrix(matrix, size);	
out:
	return err;
}

static u32 nk8_part_size(u32 block_size, int k)
{
	u32 part_size = block_size/k;
	part_size+= (block_size % k) ? 1 : 0;
	return part_size;
}

static void nk8_gen_part_ids(u8 *ids, int n)
{
	int i, j, found;
	u8 cand;

	memset(ids, 0, n);
	for (i = 0; i < n; i++) {
		while (1) {
			cand = 1 + rand_u32_up(255);
			found = 0;
			for (j = 0; j < i; j++) {
				if (cand == ids[j]) {
					found = 1;
					break;
				}
			}

			if (!found) {
				ids[i] = cand;
				break;
			}
		}
	}
}

int nk8_split_block(u8 *block, u32 block_size, int n, int k,
	u8 ***pparts, u8 **pids)
{
	u8 *svector = NULL;
	u8 **parts = NULL;
	u8 *tail = NULL;
	u8 *ids = NULL;
	int curr = 0;
	u32 tail_size = 0;
	u32 part_size;
	int i, j, m, err;

	if (n < MIN_N || k < MIN_K || block_size == 0 || n < k ||
		n > MAX_N || k > MAX_K) {
		CLOG(CL_ERR, "inv params");
		return -EINVAL;
	} 

	if (!inited) {
		CLOG(CL_ERR, "not ready");
		return -EAGAIN;
	}

	err = -ENOMEM;
	svector = crt_malloc(MAX_K*sizeof(u8));
	if (!svector) {
		goto cleanup;
	}
	tail = crt_malloc(MAX_K*sizeof(u8));
	if (!tail) {
		goto cleanup;
	}
	ids = crt_malloc(n*sizeof(u8));
	if (!ids) {
		goto cleanup;
	}
	parts = crt_malloc(n*sizeof(u8 *));
	if (!parts) {
		goto cleanup;	
	}
	memset(parts, 0, n*sizeof(u8 *));

	part_size = nk8_part_size(block_size, k);
	for (i = 0; i < n; i++) {
		parts[i] = crt_malloc(part_size);
		if (!parts[i])
			goto cleanup;
	}

	if (part_size*k > block_size) {
		tail_size = block_size - (part_size - 1)*k;
		crt_memcpy(tail, block + block_size - tail_size, tail_size);
		crt_memset(tail + tail_size, 0, k - tail_size);
		part_size-= 1;
	}		
	CLOG(CL_INF, "part_size %u tail_size %u",
		part_size, tail_size);

	nk8_gen_part_ids(ids, n);	
	for (i = 0; i < n; i++) {
		svector[0] = 1;
		for (j = 1; j < k; j++) {
			svector[j] = gf_mult(svector[j-1], ids[i]); 
		}
		for (j = 0; j < part_size; j++) {
			curr = 0;
			for (m = 0; m < k; m++) {
				curr ^= gf_mult(svector[m], block[j*k + m]);
			}
			parts[i][j] = curr;
		}			

		if (tail_size) {
			curr = 0;
			for (m = 0; m < k; m++) {
				curr ^= gf_mult(svector[m], tail[m]);
			}
			parts[i][j] = curr;
		}
	}
	*pparts = parts;
	*pids = ids;
	err = 0;
	
cleanup:
	if (tail)
		crt_free(tail);
	if (svector)
		crt_free(svector);
	if (err) {
		if (ids)
			crt_free(ids);
		if (parts) {
			for (i = 0; i < n; i++) {
				if (parts[i])
					crt_free(parts[i]);
			}
			crt_free(parts);
		}
	}

	return err;
}

int nk8_assemble_block(u8 **parts, u8 *ids, int n, int k,
		u8 *block, u32 block_size)
{
	u8 **inmatrix = NULL;
	u8 **ormatrix = NULL;
	u8 *svector = NULL;
	u8 **sparts = NULL;
	u8 *sblock = block;
	u8 *tail = NULL;
	u32 part_size;
	u32 tail_size = 0;
	int i, j, m;
	int found, num_parts;
	u8 id;
	int err;
	int curr;

	if (n < MIN_N || k < MIN_K || block_size == 0 || n < k ||
		n > MAX_N || k > MAX_K) {
		CLOG(CL_ERR, "inv params");
		return -EINVAL;
	} 

	if (!inited) {
		CLOG(CL_ERR, "not ready");
		return -EAGAIN;
	}

	part_size = nk8_part_size(block_size, k);
	if (part_size * k > block_size) {
		tail_size = block_size - (part_size - 1)*k;
		part_size-= 1;
	}

	CLOG(CL_INF, "part_size %u tail_size %u",
		part_size, tail_size);

	if (tail_size) {
		tail = crt_malloc(k*sizeof(u8));
		if (!tail) {
			err = -ENOMEM;
			goto cleanup;
		}
	}

	err = gf_alloc_matrix(&inmatrix, k);
	if (err)
		goto cleanup;

	err = gf_alloc_matrix(&ormatrix, k);
	if (err)
		goto cleanup;
	svector = crt_malloc(k*sizeof(u8));
	if (!svector) {
		err = -ENOMEM;
		goto cleanup;		
	}
	sparts = crt_malloc(k*sizeof(u8 *));
	if (!sparts) {
		err = -ENOMEM;
		goto cleanup;
	}

	for (i = 0; i < k; i++)
		ormatrix[0][i] = 1;

	num_parts = 0;
	for (i = 0; i < n; i++) {
		id = ids[i];
		found = 0;
		for (j = 0; j < i; j++) {
			if (id == ids[j]) {
				found++;
				break;
			}	
		}
		if (!found) {
			ormatrix[1][num_parts] = id;
			sparts[num_parts] = parts[i];
			num_parts++;
		}
		if (num_parts == k) { /* found k parts */
			break;
		}
	}

	if (num_parts < k) {
		CLOG(CL_ERR, "not found at least %u diff parts",
			k);
		err = -EINVAL;
		goto cleanup;
	}

	for (i = 0; i < k; i++) {
		for (m = 2; m < k; m++) {
			ormatrix[m][i] = gf_mult(ormatrix[m-1][i],
						ormatrix[1][i]);
		}
	}

	err = gf_inverse_matrix(ormatrix, inmatrix, k);
	if (err) {
		CLOG(CL_ERR, "inverse failed");
		goto cleanup;
	}

	for (j = 0; j < part_size; j++) {
		for (i = 0; i < k; i++) {
			svector[i] = sparts[i][j];
		}
		
		for (m = 0; m < k; m++) {
			curr = 0;
			for (i = 0; i < k; i++) {
				curr ^= gf_mult(svector[i],
						inmatrix[i][m]);
			}
			*sblock++ = curr;
		}
	}	

	if (tail_size) {
		sblock = tail;
		for (i = 0; i < k; i++) {
			svector[i] = sparts[i][j];
		}
		for (m = 0; m < k; m++) {
			curr = 0;
			for (i = 0; i < k; i++) {
				curr ^= gf_mult(svector[i],
						inmatrix[i][m]);
			}
			*sblock++ = curr;
		}

		crt_memcpy(block + block_size - tail_size,
			tail, tail_size);
	}

	err = 0;

cleanup:
	if (tail)
		crt_free(tail);
	if (inmatrix)
		gf_free_matrix(inmatrix, k);
	if (ormatrix)
		gf_free_matrix(ormatrix, k);
	if (svector)
		crt_free(svector);
	if (sparts)
		crt_free(sparts);
	
	return err;
}

static int nk8_test(u32 block_size, int n, int k)
{
	struct sha256_sum in_sum, out_sum;
	u8 *block = NULL;
	u8 *result = NULL;
	int err;
	u8 **parts = NULL;
	u8 **sparts = NULL;
	u8 *ids = NULL;
	u8 *sids = NULL;
	int i, j, m;
	u8 *part = NULL;
	u8 id;
	int found = 0;

	CLOG(CL_INF, "bsize %u n %d k %d", block_size, n, k);

	block = crt_malloc(block_size);
	if (!block) {
		CLOG(CL_ERR, "no mem");
		err = -ENOMEM;
		goto cleanup;
	}

	result = crt_malloc(block_size);
	if (!result) {
		CLOG(CL_ERR, "no mem");
		err = -ENOMEM;
		goto cleanup;
	}
	
	sparts = crt_malloc(k*sizeof(u8 *));
	if (!sparts) {
		CLOG(CL_ERR, "no mem");
		err = -ENOMEM;
		goto cleanup;
	}
	memset(sparts, 0, k*sizeof(u8 *));
	sids = crt_malloc(k*sizeof(u8));
	if (!sids) {
		CLOG(CL_ERR, "no mem");
		err = -ENOMEM;
		goto cleanup;
	}
	memset(sids, 0, k*sizeof(u8));

	err = crt_random_buf(block, block_size);
	if (err) {
		CLOG(CL_ERR, "can gen buf err %d", err);
		goto cleanup;
	}	

	sha256(block, block_size, &in_sum, 0);
	err = nk8_split_block(block, block_size, n, k, &parts, &ids);
	if (err) {
		CLOG(CL_ERR, "cant split block err %d", err);
		goto cleanup;
	}

	/* select k parts */
	for (i = 0; i < k; i++) {
		while (1) {
			found = 0;
			j = rand_u32_up(n);
			part = parts[j];
			id = ids[j];
			for (m = 0; m < i; m++) {
				if (sparts[m] == part) {
					found = 1;
					break;
				}
			}

			if (!found) {
				sparts[i] = part;
				sids[i] = id;
				break;	
			}
		}
	}

	err = nk8_assemble_block(sparts, sids, k, k, result, block_size);
	if (err) {
		CLOG(CL_ERR, "cant assemble block err %d", err);
		goto cleanup;
	}

	sha256(result, block_size, &out_sum, 0);
	if (0 != crt_memcmp(&in_sum, &out_sum, sizeof(out_sum))) {
		CLOG(CL_ERR, "blocks diff found");
		CLOG_BUF_SUM(block, block_size);
		CLOG_BUF_SUM(result, block_size);
		err = -EINVAL;
		goto cleanup;
	}
	CLOG(CL_INF, "success");
	err = 0;
cleanup:
	if (parts) {
		for (i = 0; i < n; i++)
			crt_free(parts[i]);
		crt_free(parts);
	}

	if (sids)
		crt_free(sids);
	if (sparts)
		crt_free(sparts);
	if (ids)
		crt_free(ids);
	if (result)
		crt_free(result);
	if (block)
		crt_free(block);
	return err;
}

int nk8_init(void)
{
	gf_init();
	inited = 1;
	gf_test_matrix();
	nk8_test(12123, 5, 3);
	nk8_test(63203, 9, 7);
	nk8_test(45021, 43, 5);
	nk8_test(4096, 6, 3);

	return 0;
}

void nk8_release(void)
{

}

