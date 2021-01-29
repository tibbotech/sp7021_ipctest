/**
 * @file    gp_chunkmem.h
 * @brief   Declaration of Chunk Memory Driver data structure & interface
 */
#ifndef _GP_CHUNKMEM_H_
#define _GP_CHUNKMEM_H_

/*
 * In 3.16, gp_chunkmem.h have been renamed as sp_chunkmem.h.
 * sdk files still include chunk memory by "gp_chunkmem.h" and use gp_chunk_*
 * handlers(not sp_chunk_*). This header is for backward compatible.
 *
 * Can not use #include <mach/sp_chunkmem.h>, sdk files may not use the same
 * header search path as kernel's.
 */

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/* Ioctl for device node definition */
#define CHUNK_MEM_IOCTL_MAGIC		'C'
#define CHUNK_MEM_ALLOC_CACHE		_IOWR(CHUNK_MEM_IOCTL_MAGIC,  1, chunk_block_t)
#define CHUNK_MEM_SHARE			_IOWR(CHUNK_MEM_IOCTL_MAGIC,  2, chunk_block_t)
#define CHUNK_MEM_FREE			_IOW (CHUNK_MEM_IOCTL_MAGIC,  3, chunk_block_t)
#define CHUNK_MEM_INFO			_IOWR(CHUNK_MEM_IOCTL_MAGIC,  4, chunk_info_t)
#define CHUNK_MEM_VA2PA			_IOWR(CHUNK_MEM_IOCTL_MAGIC,  5, chunk_block_t)
#define CHUNK_MEM_MMAP			_IOWR(CHUNK_MEM_IOCTL_MAGIC,  6, chunk_block_t)
#define CHUNK_MEM_MUNMAP		_IOW (CHUNK_MEM_IOCTL_MAGIC,  7, chunk_block_t)
#define CHUNK_MEM_ALLOC_NOCACHE		_IOWR(CHUNK_MEM_IOCTL_MAGIC, 12, chunk_block_t)
#define CHUNK_MEM_BANK			_IOWR(CHUNK_MEM_IOCTL_MAGIC, 13, chunk_block_t)

#define CHUNK_MEM_TAG_SIZE		16

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

#ifdef CHUNK_MEM_CHECK
#define chunk_memcpy(dst, src, n)		\
({						\
	unsigned int size;			\
	void *p = dst;				\
	unsigned int l = n;			\
	void *ka = sp_chunk_check(p, &size);	\
	if (ka == NULL) {			\
		printk("[chunk_memcpy][%s:%d] (%p) isn't a valid chunkmem addr!\n", __FUNCTION__, __LINE__, p); \
	} else if ((p + l) >= (ka + size)) {	\
		printk("[chunk_memcpy][%s:%d] (%p + %d) is out of boundary!\n", __FUNCTION__, __LINE__, p, l); \
	}					\
	memcpy(p, src, l);			\
})
#else
#define chunk_memcpy memcpy
#endif

#undef SP_SYNC_OPTION
#ifdef SP_SYNC_OPTION
void sp_sync_cache(void);
#define SP_SYNC_CACHE sp_sync_cache
#else
void sp_sync_cache_all(void);
#define SP_SYNC_CACHE sp_sync_cache_all
#endif

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

typedef struct chunk_block_s {
	unsigned int phy_addr;		/*!< @brief start address of memory block, physical_addr */
	void *addr;			/*!< @brief start address of memory block, user_addr */
	unsigned int size;		/*!< @brief size of memory block */
	unsigned int pool_idx;		/*!< @brief chunkmem_pool index */
	unsigned int bank_idx;		/*!< @brief dram bank index */
	char tag[CHUNK_MEM_TAG_SIZE];	/*!< @brief chunk block tag */
} chunk_block_t;

typedef struct chunk_info_s {
	unsigned int pid;		/*!< @breif [in] pid for query process used bytes, 0 for all, -1 for current proc */
	unsigned int total_bytes;	/*!< @brief [out] total size of chunkmem in bytes */
	unsigned int used_bytes;	/*!< @brief [out] used chunkmem size in bytes */
	unsigned int max_free_block_bytes; /*!< @brief [out] max free block size in bytes */
	unsigned int pool_idx;		/*!< @brief chunkmem_pool index */
	unsigned int total_pool_num;	/*!< @brief total chunkmem number */
} chunk_info_t;

/* callback function for sp_chunk_suspend save data */
typedef void (*save_data_proc)(unsigned long addr, unsigned long size);

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

void *sp_chunk_malloc_cache(unsigned int pool_idx, unsigned int id, unsigned int size);
void *sp_chunk_malloc_nocache(unsigned int pool_idx, unsigned int id, unsigned int size);
void sp_chunk_free(void *addr);
void sp_chunk_show(void);
void sp_chunk_free_all(unsigned int id);
void *sp_chunk_va(unsigned int pa);
unsigned int sp_chunk_pa(void *ka);
unsigned int sp_user_va_to_pa(void *va);
int sp_chunk_suspend(save_data_proc save_proc);
void sp_chunk_info(unsigned int pool_idx, unsigned int *base, unsigned int *size);
unsigned int sp_chunk_num(void);
int chunk_pool_idx(void *ka);
void *sp_chunk_check(void *ka, unsigned int *pSize);

#define gp_chunk_malloc_cache	sp_chunk_malloc_cache
#define gp_chunk_malloc_nocache	sp_chunk_malloc_nocache
#define gp_chunk_free		sp_chunk_free
#define gp_chunk_show		sp_chunk_show
#define gp_chunk_free_all	sp_chunk_free_all
#define gp_chunk_va		sp_chunk_va
#define gp_chunk_pa		sp_chunk_pa
#define gp_user_va_to_pa	sp_user_va_to_pa
#define gp_chunk_suspend	sp_chunk_suspend
#define gp_chunk_info		sp_chunk_info
#define gp_chunk_num		sp_chunk_num
#define gp_chunk_check		sp_chunk_check

/**************************************************************************
 *            U S E R    S P A C E    I O C T L    U S A G E              *
 **************************************************************************/
/*
 * [CHUNK_MEM_ALLOC_CACHE] : allocate chunk memory
 *   User input chunk_block_t to parameter of ioctl().
 *   Fill fields @size and @pool_idx of chunk_block_t.
 *   @size means requested size, and pool_idx means from which pool
 *   kernel should allocate memory.
 *
 *   Kernel will allocate memory and output @addr and @phy_addr fields.
 *   @addr means user space virtual address. @phy_addr means physical
 *   address.
 *
 * [CHUNK_MEM_SHARE] : not sure yet
 *   usage descrition is not ready now.
 *
 * [CHUNK_MEM_FREE] : free chunk memory
 *   User input chunk_block_t to parameter of ioctl().
 *   Fill field @addr of chunk_block_t.
 *   @addr means user space virtual address.
 *
 *   Kernel will free address. @size is not needed, kernel can handle it.
 *
 * [CHUNK_MEM_INFO] : query chunk memory infomation
 *   User input chunk_info_t(NOT chunk_block_t.) to parameter of ioctl().
 *   Fill fields @pool_idx and @pid of chunk_info_t.
 *   @pool_idx means which pool you want to query. @pid means which process
 *   id you want to query, that is, you can check memory info for specific
 *   process. If pid is 0, it is for all processes. If pid is -1, it is for
 *   current process.
 *
 *   Kernel will fill all other fields of chunk_info_t. They are all
 *   infomation field of memory.
 *
 * [CHUNK_MEM_VA2PA] : query pa by va
 *   User input chunk_block_t to parameter of ioctl().
 *   Fill field @addr of chunk_block_t.
 *   @addr means user space virtual address.
 *
 *   Kernel will fill @phy_addr, the physical address.
 *
 * [CHUNK_MEM_MMAP] : map pa with size
 *   User input chunk_block_t to parameter of ioctl().
 *   Fill fields @phy_addr and @size of chunk_block_t.
 *   @phy_addr means physical address. @size is size of this physical
 *   address.
 *
 *   Kernel will do mapping for this @phy_addr and fill @addr, which
 *   is user space virtual address.
 *
 * [CHUNK_MEM_MUNMAP] : unmap va
 *   User input chunk_block_t to parameter of ioctl().
 *   Fill field @addr of chunk_block_t.
 *
 *   Kernel will unmap this @addr. No output.
 *
 * [CHUNK_MEM_ALLOC_NOCACHE] : allocate noncache chunk memory
 *   The same as CHUNK_MEM_ALLOC_CACHE, just noncache.
 *   Refer to CHUNK_MEM_ALLOC_CACHE for usage.
 *
 * [CHUNK_MEM_BANK] : query memory bank index
 *   User input chunk_block_t to parameter of ioctl().
 *   Fill field @addr of chunk_block_t.
 *
 *   Kernel will output @bank_idx, which is meory bank id.
 */

#endif /* _GP_CHUNKMEM_H_ */
