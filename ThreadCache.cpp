#include "ThreadCache.h"
#include "CentralCache.h"

// �������
void* ThreadCache::Allocate(size_t size)
{
	// С��256KB
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeRule::RoundUp(size);// ����֮����ڴ���С
	size_t idx = SizeRule::Index(size);// ��������Ͱ��λ��
	if (!_freeLists[idx].empty())
	{
		// ��Ϊ�վʹ����������ȡ
		return _freeLists[idx].pop_front();
	}
	else
	{
		return FetchFromCentralCache(idx, alignSize);
	}
}

// �ͷŶ���ʱ����������������յ�central cache
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr, * end = nullptr;
	// ȡ��һ���������ڴ�
	list.pop_range(start, end, list.max_size());
	
	CentralCache::GetSingleton().ReleaseListToSpans(start, size);
}

// �ͷ��ڴ����
void ThreadCache::deAllocate(void* ptr, size_t size)
{
	assert(ptr && size <= MAX_BYTES);
	// �ҵ���������Ͱ�����ȥ
	size_t idx = SizeRule::Index(size);
	_freeLists[idx].push_front(ptr);
	// �����ȴ���һ�� ����������ڴ�ͻ���
	if (_freeLists[idx].size() >= _freeLists[idx].max_size())
	{
		ListTooLong(_freeLists[idx], size);
	}
}

// �����Ļ����ȡ����
void* ThreadCache::FetchFromCentralCache(size_t idx, size_t size)
{
	// ����ʼ���������㷨
	size_t batchNum = min(SizeRule::NumMoveSize(size), _freeLists[idx].max_size());
	if (batchNum == _freeLists[idx].max_size())
	{
		_freeLists[idx].max_size() += 1;
	}
	void* start = nullptr, * end = nullptr;
	// span��һ������ô�࣬ʵ����actualsize
	size_t actualNum = CentralCache::GetSingleton().FetchRangObj(start, end, batchNum, size);
	assert(actualNum >= 1);// ���ٵ�����һ��
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		// ͷ���أ�ʣ�µĲ�������������
		_freeLists[idx].push_range(NextObj(start), end, actualNum - 1);
		return start;
	}
}