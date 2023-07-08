#include "ThreadCache.h"
#include "CentralCache.h"

// 申请对象
void* ThreadCache::Allocate(size_t size)
{
	// 小于256KB
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeRule::RoundUp(size);// 对齐之后的内存块大小
	size_t idx = SizeRule::Index(size);// 自由链表桶的位置
	if (!_freeLists[idx].empty())
	{
		// 不为空就从自由链表获取
		return _freeLists[idx].pop_front();
	}
	else
	{
		return FetchFromCentralCache(idx, alignSize);
	}
}

// 释放对象时发现链表过长，回收到central cache
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr, * end = nullptr;
	// 取出一个批量的内存
	list.pop_range(start, end, list.max_size());
	
	CentralCache::GetSingleton().ReleaseListToSpans(start, size);
}

// 释放内存对象
void ThreadCache::deAllocate(void* ptr, size_t size)
{
	assert(ptr && size <= MAX_BYTES);
	// 找到自由链表桶插入进去
	size_t idx = SizeRule::Index(size);
	_freeLists[idx].push_front(ptr);
	// 链表长度大于一次 批量申请的内存就回收
	if (_freeLists[idx].size() >= _freeLists[idx].max_size())
	{
		ListTooLong(_freeLists[idx], size);
	}
}

// 从中心缓存获取对象
void* ThreadCache::FetchFromCentralCache(size_t idx, size_t size)
{
	// 慢开始反馈调节算法
	size_t batchNum = min(SizeRule::NumMoveSize(size), _freeLists[idx].max_size());
	if (batchNum == _freeLists[idx].max_size())
	{
		_freeLists[idx].max_size() += 1;
	}
	void* start = nullptr, * end = nullptr;
	// span不一定有那么多，实际有actualsize
	size_t actualNum = CentralCache::GetSingleton().FetchRangObj(start, end, batchNum, size);
	assert(actualNum >= 1);// 至少得申请一个
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		// 头返回，剩下的插入自由链表中
		_freeLists[idx].push_range(NextObj(start), end, actualNum - 1);
		return start;
	}
}