#include "CentralCache.h"
#include "PageCache.h"

// 定义单例模式
CentralCache CentralCache::_cSingle;

// 获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	Span* it = list.begin();
	while (it != list.end())
	{
		if (it->_freeList != nullptr)// span挂有对象
		{
			return it;
		}
		it = it->_next;
	}
	// 进入page cache之前先把桶锁解掉，防止阻塞
	list.unlock();
	// 无空闲的span,向page cache要
	PageCache::GetSingleton().lock();
	Span* span = PageCache::GetSingleton().NewSpan(SizeRule::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetSingleton().unlock();
	// Span的起始地址
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	// Span大小
	size_t bytes = span->_n << PAGE_SHIFT;
	// 结尾地址
	char* end = start + bytes;
	// 把大块内存切成自由链表挂到span
	// 切下来一块当头节点
	span->_freeList = start;
	start += size;
	// 尾节点方便尾插
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;
	// 加桶锁
	list.lock();
	// 切好之后挂回桶
	list.push_front(span);
	return span;
}

// 从中心缓存中获取一定数量的对象给threadcache
size_t CentralCache::FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	// 桶的位置
	size_t idx = SizeRule::Index(size);
	// 加上桶锁
	_spanList[idx].lock();
	// 首先找到一个非空的span
	Span* span = GetOneSpan(_spanList[idx], size);
	assert(span);
	assert(span->_freeList);
	// 开始切分
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualsize = 1;// 真实对象数量
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		i++;
		actualsize++;
		end = NextObj(end);
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCnt += actualsize;
	_spanList[idx].unlock();
	return actualsize;
}


// 将一定数量从thread cache回收的对象挂到对应的span
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t idx = SizeRule::Index(size);
	// 桶锁
	_spanList[idx].lock();
	while (start)
	{
		void* next = NextObj(start);
		// 根据地址算出页号，找到对应的span
		Span* span = PageCache::GetSingleton().MapObjToSpan(start);
		// 头插入span的_freeList中
		NextObj(start) = span->_freeList;
		span->_freeList = start;

		span->_useCnt--;
		// span切出去的对象全部换回来了
		// 说明这个span可以还给page cache了
		if (span->_useCnt == 0)
		{
			_spanList[idx].erase(span);
			// 只需要span管理的这些内存回来了即可
			// _pageId和_n不能变
			span->_freeList = nullptr;
			span->_next = span->_prev = nullptr;
			// 解桶锁，加大锁
			_spanList[idx].unlock();
			PageCache::GetSingleton().lock();
			// 归还span到page cache
			PageCache::GetSingleton().ReleaseSpanToPageCache(span);
			PageCache::GetSingleton().unlock();
			_spanList[idx].lock();
		}
		start = next;
	}
	_spanList[idx].unlock();
}