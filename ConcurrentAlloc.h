#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "FixedPool.h"

// ����
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)// ����256kb
	{
		size_t alignSize = SizeRule::RoundUp(size);
		// ҳ��
		size_t kPage = alignSize >> PAGE_SHIFT;
		PageCache::GetSingleton().lock();
		Span* span = PageCache::GetSingleton().NewSpan(kPage);
		span->_objSize = size;
		PageCache::GetSingleton().unlock();
		// ��õ�ַ
		void* addr = (void*)(span->_pageId << PAGE_SHIFT);
		return addr;
	}
	else
	{
		// ÿ���߳�������ȡThreadCache����
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

// �ͷ�
static void ConcurrentFree(void* ptr)
{
	// ͨ����ַ��ȡspan
	Span* span = PageCache::GetSingleton().MapObjToSpan(ptr);
	size_t size = span->_objSize;
	// ����32ҳ
	if (size > MAX_BYTES)
	{
		// ͨ��ӳ���ϵ�ҵ�span
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