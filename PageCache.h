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

	// 获取从对象到span的映射
	Span* MapObjToSpan(void* obj);
	// central cache归还span给page cache
	void ReleaseSpanToPageCache(Span* span);

	// 加锁和解锁
	void lock();
	void unlock();

	// 获取n页Span
	Span* NewSpan(size_t k);
private:
	SpanList _spanLists[NPAGES];
	// std::unordered_map<PAGE_ID, Span*> _idSpan;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpan;
	std::mutex _mtx;
	
	FixedPool<Span> _spanPool;
private:
	// 构造私有+防拷贝
	PageCache() {}
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	// 单例
	static PageCache _pSingle;
};