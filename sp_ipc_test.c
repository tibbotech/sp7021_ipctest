#define IPC_SERVER
#define IPC_CLIENT
#define EXAMPLE_CODE_FOR_USER_GUIDE

#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "gp_chunkmem.h"
#include "gp_cache.h"
#include "sp_ipc_api.c"

#define MAX_THREADS		8
static int fd_chunkmem = -1;
static int fd_cache = -1;

void *chunk_malloc(int len, u32 *pa)
{
	chunk_block_t b;

	if (fd_chunkmem < 0) {
		fd_chunkmem = open("/dev/chunkmem", O_RDWR);
	}

	memset(&b, 0, sizeof(b));
	b.size = len;
	//ioctl(fd_chunkmem, CHUNK_MEM_ALLOC, &b); //tonyh test 
	*pa = b.phy_addr;

	return b.addr;
}

void chunk_free(void *p)
{
	chunk_block_t b;

	b.addr = p;
	ioctl(fd_chunkmem, CHUNK_MEM_FREE, &b);
}

void cache_flush(void *va, u32 size)
{
	gp_cache_address_t t;

	if (fd_cache < 0) {
		fd_cache = open("/dev/cache", O_RDWR);
	}

	t.start = (u32)va;
	t.size = size;
	ioctl(fd_cache, DCACHE_CLEAN_RANGE, &t);
}

void cache_invalidate(void *va, u32 size)
{
	gp_cache_address_t t;

	if (fd_cache < 0) {
		fd_cache = open("/dev/cache", O_RDWR);
	}

	t.start = (u32)va;
	t.size = size;
	ioctl(fd_cache, DCACHE_INVALIDATE_RANGE, &t);
}

typedef struct {
	u32 addr;
	u32 size;
} bigdata_t;

u32 rpc_test_handler(int cmd, void *data, int len)
{
	u8 c = cmd & 0x7F;

	//printf("TEST_HANDLER...\n\n");
	if (len == sizeof(bigdata_t) && *((u32 *)data) != 0x11111111) {
		bigdata_t *d = (bigdata_t *)data;
		void *p = ipc_mmap(d->addr, d->size);
		if (!p) {
			printf("ipc_mmap fail(%d) %s\n", errno, strerror(errno));
			hex_dump(data, len);
			return IPC_FAIL;
		}
		cache_invalidate(p, d->size);
		memset(p, c, d->size);
		cache_flush(p, d->size);
		if (ipc_munmap(d->addr, d->size, p)) {
			printf("ipc_munmap fail(%d) %s\n", errno, strerror(errno));
			hex_dump(data, len);
		}
	}
	else {
		memset(data, c, len);
	}

	return IPC_SUCCESS;
}

int check_data(int cmd, u8 *p, int len)
{
	int i = len;
	u8 c = cmd & 0x7F;

	while (i--) {
		if (p[i] != c) {
			printf("!!!!! DATA ERROR !!!!!\n");
			hex_dump(p, len);
			return IPC_FAIL;
		}
	}

	return IPC_SUCCESS;
}

int rpc_test(int cmd, int len)
{
	int ret;
	void *p = NULL;

	if (len > IPC_DATA_SIZE_MAX) {
		bigdata_t d;
		p = chunk_malloc(len, &d.addr);
		d.size = len;
		memset(p, 0x11, len);
		cache_flush(p, len);
	
		ret = IPC_FunctionCall(cmd, &d, sizeof(d));
		if (ret == IPC_SUCCESS) {
			ret = check_data(cmd, (u8 *)p, len);
		}
		chunk_free(p);
	}
	else {
		if (len) {
			p = malloc(len);
			memset(p, 0x11, len);
		}
		ret = IPC_FunctionCall(cmd, p, len);
		if (ret == IPC_SUCCESS) {
			ret = check_data(cmd, (u8 *)p, len);
		}
		if (p) free(p);
	}

	return ret;
}

void *rpc_test_thread(void *param)
{
	int ret = IPC_SUCCESS;
	int i = (int)param;
	u32 t = (u32)pthread_self();

	printf("Start %s [%08x] ...\n", __FUNCTION__, t);
	while (i--) {
		int cmd = rand() & 0x3FF;
		int len = rand() % (IPC_DATA_SIZE_MAX * 2);

		ret = rpc_test(cmd , len);
		if (ret != IPC_SUCCESS) {
			printf("[%08x] %d %d: %d\n",t, cmd, len, ret);
			if (ret != IPC_FAIL_INVALID && ret != IPC_FAIL_UNSUPPORT) {
				break;
			}
		}
	}
	printf("Stop %s [%08x]: %d\n", __FUNCTION__, t, ret);

	return (void *)ret;
}

#ifdef EXAMPLE_CODE_FOR_USER_GUIDE
int main(int argc, char *argv[])
{
	int ret = 0;
	char data[2];	
	static int fd = -1;
	
	if (argc < 2) goto err;

	switch (argv[1][0]) {
		case 'r':	// read
			if (fd < 0) 
				fd = open("/dev/sp_ipc", O_RDWR);
			data[0] = atoi(argv[2]);
			data[1] = atoi(argv[3]);
			ret = read(fd, data, sizeof(data));
			close(fd);	
			break;

		
		case 'w':	// write	
			if (fd < 0) 
				fd = open("/dev/sp_ipc", O_RDWR);	
			data[0] = atoi(argv[2]);
			data[1] = atoi(argv[3]);
			ret = write(fd, data, sizeof(data));
			close(fd);	
			break;
		default:
			goto err;	
	}
	printf("RET = %d\n", ret);
	return ret;

err:
	printf("Usage:\n");
	printf("    Read : ipc r <id> <length>\n");
	printf("    Write  : ipc w <id> <data>\n");
	return -1;
}

#else 
int main(int argc, char *argv[])
{
	int ret = 0;

	if (argc < 2) goto err;

	switch (argv[1][0]) {
	case 's':	// SERVER
		{
			int sid = (argc > 2) ? atoi(argv[2]) : 0;
 			int i = RPC_CMD_MAX + 1;
			
			if (sid >= SERVER_NUMS) goto err;
			
			while (i--) {
				IPC_RegFunc(i, rpc_test_handler);
			}
			
			IPC_Init(sid);
			IPC_Finalize();
		}
		break;
	
	case 'm':	// MAILBOX
    	ret = IPC_MBFunctionCall(atoi(argv[2]), atoi(argv[3]));
		break;

	case 'c':	// CLIENT
		{
			int cmd, len;

			cmd = atoi(argv[2]);
			len = (argc > 3) ? atoi(argv[3]) : 8;
			ret = rpc_test(cmd, len);
		}
		break;

	case 'b':	// BURN IN
		{
			pthread_t th[MAX_THREADS];
			int l, t, i;

			l = atoi(argv[2]);						// loops
			t = (argc > 3) ? atoi(argv[3]) : 1;		// threads
			if (t > MAX_THREADS) t = MAX_THREADS;

			srand(time(NULL));
			i = t;
			while (i--) {
				//pthread_create(&th[i], NULL, rpc_test_thread, (void *)l);
			}
			i = t;
			while (i--) {
				int r;
				//pthread_join(th[i], (void **)&r);
				ret += r;
			}
		}
		break;

	default:
		goto err;
	}

	printf("RET = %d\n", ret);
	return ret;

err:
	printf("Usage:\n");
#if (SERVER_NUMS == 1)
	printf("    Server  : ipc s\n");
#else
	printf("    Server  : ipc s <server_id:0-%d>\n", SERVER_NUMS - 1);
#endif
	printf("    Mailbox : ipc m <id> <data>\n");
	printf("    Client  : ipc c <cmd> <data_len>\n");
	printf("    Burn in : ipc b <loops> <threads>\n");
	
	return -1;
}
#endif 
