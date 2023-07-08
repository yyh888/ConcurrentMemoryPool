#pragma once
#include "Common.h"
#include "FixedPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache& GetSingleton()
	{
		return _pSingle;
	}

	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjToSpan(void* obj);
	// central cache�黹span��page cache
	void ReleaseSpanToPageCache(Span* span);

	// �����ͽ���
	void lock();
	void unlock();

	// ��ȡnҳSpan
	Span* NewSpan(size_t k);
private:
	SpanList _spanLists[NPAGES];
	// std::unordered_map<PAGE_ID, Span*> _idSpan;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpan;
	std::mutex _mtx;
	
	FixedPool<Span> _spanPool;
private:
	// ����˽��+������
	PageCache() {}
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	// ����
	static PageCache _pSingle;
};