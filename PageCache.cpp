#include "PageCache.h"

// 定义单例模式
PageCache PageCache::_pSingle;

void PageCache::lock()
{
	_mtx.lock();
}

void PageCache::unlock()
{
	_mtx.unlock();
}

// 获取k页Span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	// 大于128页
	if (k > NPAGES - 1)
	{
		// 直接向堆区获取内存
		void* addr = SystemAlloc(k);
		// Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)addr >> PAGE_SHIFT;
		span->_n = k;
		// 建立映射关系
		// _idSpan[span->_pageId] = span;
		_idSpan.set(span->_pageId, span);
		return span;
	}
	else
	{
		// 直接找k页的桶
		if (!_spanLists[k].empty())
		{
			Span* kspan = _spanLists[k].pop_front();
			// 建立id 与 span的映射关系
			for (PAGE_ID i = 0; i < kspan->_n; i++)
			{
				// _idSpan[kspan->_pageId + i] = kspan;
				_idSpan.set(kspan->_pageId + i, kspan);
			}
			return kspan;
		}
		// 第k个桶是空的，检查后边的桶
		for (size_t i = k + 1; i < NPAGES; i++)
		{
			if (!_spanLists[i].empty())
			{
				Span* nspan = _spanLists[i].pop_front();
				// 切分成k页和n - k页
				// Span* kspan = new Span;
				Span* kspan = _spanPool.New();
				// 从nspan的头部切一个k页span
				kspan->_pageId = nspan->_pageId;// 页号
				kspan->_n = k;// 页数
				nspan->_pageId += k;
				nspan->_n -= k;
				// 把剩下的页再挂起来
				_spanLists[nspan->_n].push_front(nspan);
				// 存储nspan的首尾页号与span的映射关系，方便page cache合并查找
				//_idSpan[nspan->_pageId] = nspan;
				//_idSpan[nspan->_pageId + nspan->_n - 1] = nspan;
				_idSpan.set(nspan->_pageId, nspan);
				_idSpan.set(nspan->_pageId + nspan->_n - 1, nspan);
				// 建立id 与 span的映射关系
				for (PAGE_ID i = 0; i < kspan->_n; i++)
				{
					//_idSpan[kspan->_pageId + i] = kspan;
					_idSpan.set(kspan->_pageId + i, kspan);
				}
				return kspan;
			}
		}
		// 后面也没有span
		// 向堆要128页span
		// Span* spanfromheap = new Span;
		Span* spanfromheap = _spanPool.New();
		void* ptr = SystemAlloc(NPAGES - 1);
		// 地址转页号
		spanfromheap->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		spanfromheap->_n = NPAGES - 1;
		_spanLists[NPAGES - 1].push_front(spanfromheap);
		// 有了大块span后递归调用自己
		return NewSpan(k);
	}
}


Span* PageCache::MapObjToSpan(void* obj)
{
	// 通过地址算页号
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
	/*lock();
	if (_idSpan.count(id))
	{
		unlock();
		return _idSpan[id];
	}
	else
	{
		unlock();
		assert(false);
		return nullptr;
	}*/
	auto it = (Span*)_idSpan.get(id);
	assert(it != nullptr);// 没有找到
	return it;
}

// central cache归还span给page cache
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 大于128页
	if (span->_n > NPAGES - 1)
	{
		void* addr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(addr);
		// delete span;
		_spanPool.Delete(span);
	}
	else
	{
		// 对前后页尝试合并
		// 向前合并
		while (true)
		{
			PAGE_ID preId = span->_pageId - 1;
			// 无前面的页号
			/*if (!_idSpan.count(preId))
			{
				break;
			}
			Span* preSpan = _idSpan[preId];*/
			auto it = (Span*)_idSpan.get(preId);
			if (it == nullptr) break;
			Span* preSpan = it;
			// 前面的页号被使用
			if (preSpan->_isUse == true)
			{
				break;
			}
			// span超过了128页
			if (preSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			// 合并
			span->_pageId = preSpan->_pageId;
			span->_n += preSpan->_n;
			// 将preSpan从对应的双向链表删除
			_spanLists[preSpan->_n].erase(preSpan);
			// 不是释放自由链表，而是删除申请的preSpan的结构体
			// delete preSpan;
			_spanPool.Delete(preSpan);
		}
		// 向后合并
		while (true)
		{
			PAGE_ID nextId = span->_pageId + span->_n;
			// 无后边的页号
			/*if (!_idSpan.count(nextId))
			{
				break;
			}
			Span* nextSpan = _idSpan[nextId];*/
			auto it = (Span*)_idSpan.get(nextId);
			if (it == nullptr) break;
			Span* nextSpan = it;
			// 后面的页号被使用
			if (nextSpan->_isUse == true)
			{
				break;
			}
			// span超过了128页
			if (nextSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			// 合并
			// 起始页号不用变
			span->_n += nextSpan->_n;
			// 将nextSpan从对应的双向链表删除
			_spanLists[nextSpan->_n].erase(nextSpan);
			// delete nextSpan;
			_spanPool.Delete(nextSpan);
		}

		// 把span挂起来
		_spanLists[span->_n].push_front(span);
		span->_isUse = false;
		// 把首尾id和span关联起来，方便其他的span合并
		//_idSpan[span->_pageId] = span;
		//_idSpan[span->_pageId + span->_n - 1] = span;
		_idSpan.set(span->_pageId, span);
		_idSpan.set(span->_pageId + span->_n - 1, span);
	}
}