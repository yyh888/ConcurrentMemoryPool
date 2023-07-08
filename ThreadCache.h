#pragma once
#include "Common.h"

class ThreadCache
{
public:
	// �������
	void* Allocate(size_t size);
	// �ͷ��ڴ����
	void deAllocate(void* ptr, size_t size);
	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t idx, size_t size);
	// �ͷŶ���ʱ����������������յ�central cache
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;