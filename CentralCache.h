#pragma once
#include "Common.h"

class CentralCache
{
public:
	static CentralCache& GetSingleton()
	{
		return _cSingle;
	}

	// 从中心缓存中获取一定数量的对象给threadcache
	size_t FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size);
	// 获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t size);
	// 将一定数量从thread cache回收的对象挂到对应的span
	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanList[NFREELISTS];
private:
	// 构造私有+防拷贝
	CentralCache(){}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	// 单例
	static CentralCache _cSingle;
};