#include "CentralCache.h"
#include "PageCache.h"

// ���嵥��ģʽ
CentralCache CentralCache::_cSingle;

// ��ȡһ���ǿյ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	Span* it = list.begin();
	while (it != list.end())
	{
		if (it->_freeList != nullptr)// span���ж���
		{
			return it;
		}
		it = it->_next;
	}
	// ����page cache֮ǰ�Ȱ�Ͱ���������ֹ����
	list.unlock();
	// �޿��е�span,��page cacheҪ
	PageCache::GetSingleton().lock();
	Span* span = PageCache::GetSingleton().NewSpan(SizeRule::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetSingleton().unlock();
	// Span����ʼ��ַ
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	// Span��С
	size_t bytes = span->_n << PAGE_SHIFT;
	// ��β��ַ
	char* end = start + bytes;
	// �Ѵ���ڴ��г���������ҵ�span
	// ������һ�鵱ͷ�ڵ�
	span->_freeList = start;
	start += size;
	// β�ڵ㷽��β��
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;
	// ��Ͱ��
	list.lock();
	// �к�֮��һ�Ͱ
	list.push_front(span);
	return span;
}

// �����Ļ����л�ȡһ�������Ķ����threadcache
size_t CentralCache::FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	// Ͱ��λ��
	size_t idx = SizeRule::Index(size);
	// ����Ͱ��
	_spanList[idx].lock();
	// �����ҵ�һ���ǿյ�span
	Span* span = GetOneSpan(_spanList[idx], size);
	assert(span);
	assert(span->_freeList);
	// ��ʼ�з�
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualsize = 1;// ��ʵ��������
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


// ��һ��������thread cache���յĶ���ҵ���Ӧ��span
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t idx = SizeRule::Index(size);
	// Ͱ��
	_spanList[idx].lock();
	while (start)
	{
		void* next = NextObj(start);
		// ���ݵ�ַ���ҳ�ţ��ҵ���Ӧ��span
		Span* span = PageCache::GetSingleton().MapObjToSpan(start);
		// ͷ����span��_freeList��
		NextObj(start) = span->_freeList;
		span->_freeList = start;

		span->_useCnt--;
		// span�г�ȥ�Ķ���ȫ����������
		// ˵�����span���Ի���page cache��
		if (span->_useCnt == 0)
		{
			_spanList[idx].erase(span);
			// ֻ��Ҫspan�������Щ�ڴ�����˼���
			// _pageId��_n���ܱ�
			span->_freeList = nullptr;
			span->_next = span->_prev = nullptr;
			// ��Ͱ�����Ӵ���
			_spanList[idx].unlock();
			PageCache::GetSingleton().lock();
			// �黹span��page cache
			PageCache::GetSingleton().ReleaseSpanToPageCache(span);
			PageCache::GetSingleton().unlock();
			_spanList[idx].lock();
		}
		start = next;
	}
	_spanList[idx].unlock();
}