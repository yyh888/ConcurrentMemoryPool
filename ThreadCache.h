#pragma once
#include "Common.h"

class ThreadCache
{
public:
	// 申请对象
	void* Allocate(size_t size);
	// 释放内存对象
	void deAllocate(void* ptr, size_t size);
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t idx, size_t size);
	// 释放对象时发现链表过长，回收到central cache
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;