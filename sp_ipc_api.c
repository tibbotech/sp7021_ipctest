/**
 * @file    sp_ipc_api.c
 * @brief   Implement of Sunplus IPC API.
 * @author  qinjian
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "sp_ipc.h"

/********************************* COMMON *********************************/

static int ipc_fd(void)
{
	static int fd = -1;
	if (fd < 0) {
		fd = open("/dev/sp_ipc", O_RDWR);
	}
	return fd;
}

int rpc_read(rpc_t *rpc)
{
	int ret = read(ipc_fd(), rpc, sizeof(rpc_t));
	return (ret == sizeof(rpc_t) ? IPC_SUCCESS : errno);
}

int rpc_write(rpc_t *rpc)
{
	int ret = write(ipc_fd(), rpc, sizeof(rpc_t));
	return (ret == sizeof(rpc_t) ? IPC_SUCCESS : errno);
}

void *ipc_mmap(u32 pa, u32 size)
{
	u32 mask = getpagesize() - 1;
	u32 offset = pa & mask;
	void *va = mmap(NULL,
		(offset + size + mask) & ~mask,
		(PROT_READ | PROT_WRITE),
		MAP_SHARED,
		ipc_fd(),
		pa & ~mask);

	return (va == MAP_FAILED) ? NULL : (va + offset);
}

int ipc_munmap(u32 pa, u32 size, void *va)
{
	u32 mask = getpagesize() - 1;
	u32 offset = pa & mask;

	return munmap(va - offset, (offset + size + mask) & ~mask);
}

#ifdef IPC_SERVER
/********************************* SERVER *********************************/

static ipc_func funcs[RPC_CMD_MAX + 1];

static void rpc_do_request(rpc_t *rpc)
{
	int cmd = rpc->CMD & RPC_CMD_MAX;
	void *p = (rpc->DATA_LEN > RPC_DATA_SIZE) ?	rpc->DATA_PTR : rpc->DATA;
	printf("rpc_do_request\n");

	if (cmd > RPC_CMD_MAX || !funcs[cmd]) {	// check cmd
		print("IPC_FAIL_UNSUPPORT\n\n");
		rpc->CMD = IPC_FAIL_UNSUPPORT;
	}
	else {		
		printf("rpc_do_request_0\n");
		rpc->CMD = funcs[cmd](cmd, p, rpc->DATA_LEN);
	}

	rpc->F_DIR = RPC_RESPONSE;
	rpc_write(rpc); 						// write response
}
static void rpc_server(int id)
{
	printf("Start %s #%d ...\n", __FUNCTION__, id);
	write(ipc_fd(), id, sizeof(id));//register server
	while (1) {
		static rpc_t rpc;
		static u8 data[IPC_DATA_SIZE_MAX];
		rpc.DATA_PTR = data;
		rpc.REQ_H = (void*)id;				// server id
 		rpc_read(&rpc);	// wait & read request
		rpc_dump("SVR", &rpc);
		rpc_do_request(&rpc);				// do request
	}
}

/******************************* SERVER API *******************************/

int IPC_Init(int server_id)
{
	printf("6660\n");
	rpc_server(server_id);
	return IPC_SUCCESS;
}

void IPC_Finalize(void)
{
}
int IPC_RegFunc(int cmd, ipc_func pfIPCHandler)
{
	funcs[cmd] = pfIPCHandler;
	return IPC_SUCCESS;
}

int IPC_UnregFunc(int cmd)
{
	funcs[cmd] = NULL;
	return IPC_SUCCESS;
}
#endif

#ifdef IPC_CLIENT
/******************************* CLIENT API *******************************/

#define REG_BASE	0x9C000000
#define MSLEEP(n)	usleep((n)*1000)

int IPC_MBFunctionCall(int id, unsigned int data)
{
	static ipc_t *IPC_LOCAL = NULL;
	u32 mask;
	int ret;

	if (IPC_LOCAL == NULL) {
		u32 pa = REG_BASE + 258 * 32 * 4; // G258
		IPC_LOCAL = (ipc_t *)ipc_mmap(pa, sizeof(ipc_t));
		if (IPC_LOCAL == NULL) {
			return IPC_FAIL_NODEV;
		}
	}
	
	printf("id= %d>\n", id);
	printf("data= %d>\n", data);

	if ((u32)id >= MAILBOX_NUM) {
		return IPC_FAIL_INVALID;
	}

	mask = (1 << (24 + id));
	#if 0
	WAIT_IPC_WRITEABLE(mask);
	#else 
	{
		int ret = IPC_SUCCESS;
		int _i = IPC_WRITE_TIMEOUT;
		while (IPC_LOCAL->F_RW & (mask)) {
			MSLEEP(1);
			if (!_i--) {
				printf("write IPC HW timeout!\n");
				ret = IPC_FAIL_HWTIMEOUT;
				break;
			}
		}
		return ret;
	}
	#endif
	IPC_LOCAL->MBOX[id] = data;

	return IPC_SUCCESS;
}

int IPC_FunctionCall(int cmd, void *data, int len)
{
	int ret;
	rpc_t _rpc;
	rpc_t *rpc = &_rpc;

	if ((u32)cmd > 1023 || (u32)len > IPC_DATA_SIZE_MAX) {
		return IPC_FAIL_INVALID;
	}

	/* init rpc */
	rpc->F_DIR    = RPC_REQUEST;//0
	rpc->F_TYPE   = REQ_WAIT_REP;//0
	rpc->CMD      = cmd;
	rpc->DATA_LEN = len;
	if (len <= RPC_DATA_SIZE) {
		memcpy(rpc->DATA, data, len);
	}
	else {
		rpc->DATA_PTR = data;
	}

	/* do rpc */
	rpc_dump("REQ", rpc);
	ret = rpc_write(rpc);				// write request & wait response
	rpc_dump("RES", rpc);

	/* return data */
	if (ret == IPC_SUCCESS) {
		if (len <= RPC_DATA_SIZE) {
			memcpy(data, rpc->DATA, len);
		}
	}

	return ret;
}
#endif
