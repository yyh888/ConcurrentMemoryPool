#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "FixedPool.h"

// 申请
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)// 大于256kb
	{
		size_t alignSize = SizeRule::RoundUp(size);
		// 页数
		size_t kPage = alignSize >> PAGE_SHIFT;
		PageCache::GetSingleton().lock();
		Span* span = PageCache::GetSingleton().NewSpan(kPage);
		span->_objSize = size;
		PageCache::GetSingleton().unlock();
		// 获得地址
		void* addr = (void*)(span->_pageId << PAGE_SHIFT);
		return addr;
	}
	else
	{
		// 每个线程无锁获取ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			static std::mutex _tcMtx;
			static FixedPool<ThreadCache> _tcPool;
			_tcMtx.lock();
			//pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = _tcPool.New();
			_tcMtx.unlock();
		}
		return pTLSThreadCache->Allocate(size);
	}
}

// 释放
static void ConcurrentFree(void* ptr)
{
	// 通过地址获取span
	Span* span = PageCache::GetSingleton().MapObjToSpan(ptr);
	size_t size = span->_objSize;
	// 大于32页
	if (size > MAX_BYTES)
	{
		// 通过映射关系找到span
		Span* span = PageCache::GetSingleton().MapObjToSpan(ptr);

		PageCache::GetSingleton().lock();
		PageCache::GetSingleton().ReleaseSpanToPageCache(span);
		PageCache::GetSingleton().unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->deAllocate(ptr, size);
	}
}