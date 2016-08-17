#include "buf-pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

Buf *busyHead = NULL;
Buf *freeHead = NULL;
size_t totalBuffers = 0;
size_t freeBuffers = 0;
size_t busyBuffers = 0;

inline void addToBusyBuffers(Buf *buf) {
	// printf("addToBusyBuffers\n");
	busyBuffers++;
	freeBuffers--;
	if(!busyHead) {
		// printf("no busyHead\n");
		buf->prev = NULL;
		buf->next = NULL;
		busyHead = buf;
	} else {
		// printf("new busyHead\n");
		buf->prev = NULL;
		buf->next = busyHead;
		busyHead->prev = buf;
		busyHead = buf;
	}
}

inline void printStats() {
	static int req = 0;
	if(req++ % 10000 == 0)
		printf("%zu free buffers, %zu busy buffers, totally %zu buffers\n", freeBuffers, busyBuffers, totalBuffers);
}

void* allocBuf(size_t size) {
	printf("allocBuf %zu\n", size);
	if(size != 65536){
		printf("!!! size is not 65536, no implemented\n");
	}
	Buf *buf = freeHead;
	if(!buf) {
		totalBuffers++;
		freeBuffers++;
		// printf("no free buf is available\n");
		buf = (Buf*)malloc(sizeof(Buf) + size);
		buf->size = size;
		buf->isFree = false;
		// printf("now %d buffers total\n", totalBuffers);
		addToBusyBuffers(buf);
	} else {
		// printf("there is freeHead\n");
		freeHead = buf->next;
		buf->isFree = false;
		addToBusyBuffers(buf);
	}
	printStats();
	return (char*)buf + sizeof(Buf);
}

void freeBuf(void *ptr) {
	printf("freeBuf\n");
	Buf *buf = (Buf*)((char*)ptr - sizeof(Buf));
	if(buf->isFree) {
		printf("buffer fried twice, size %zu %p!!\n", buf->size, ptr);
		return;
	}
	buf->isFree = true;
	if(buf->prev) {
		buf->prev->next = buf->next;
	}
	if(buf->next) {
		buf->next->prev = buf->prev;
	}
	buf->prev = NULL;
	buf->next = freeHead;
	if(freeHead) {
		// printf("there is buffer in freeHead\n");
		freeHead->prev = buf;
	}
	freeHead = buf;
	busyBuffers--;
	freeBuffers++;
	printStats();
}