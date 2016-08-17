#ifndef BUF_POOL_H
#define BUF_POOL_H

#include <stddef.h>


struct Buf {
	Buf* prev;
	Buf* next;
	size_t size;
	bool isFree;
};

void* allocBuf(size_t size);
void freeBuf(void *ptr);

#endif