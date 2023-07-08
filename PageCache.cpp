#include "PageCache.h"

// ���嵥��ģʽ
PageCache PageCache::_pSingle;

void PageCache::lock()
{
	_mtx.lock();
}

void PageCache::unlock()
{
	_mtx.unlock();
}

// ��ȡkҳSpan
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	// ����128ҳ
	if (k > NPAGES - 1)
	{
		// ֱ���������ȡ�ڴ�
		void* addr = SystemAlloc(k);
		// Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)addr >> PAGE_SHIFT;
		span->_n = k;
		// ����ӳ���ϵ
		// _idSpan[span->_pageId] = span;
		_idSpan.set(span->_pageId, span);
		return span;
	}
	else
	{
		// ֱ����kҳ��Ͱ
		if (!_spanLists[k].empty())
		{
			Span* kspan = _spanLists[k].pop_front();
			// ����id �� span��ӳ���ϵ
			for (PAGE_ID i = 0; i < kspan->_n; i++)
			{
				// _idSpan[kspan->_pageId + i] = kspan;
				_idSpan.set(kspan->_pageId + i, kspan);
			}
			return kspan;
		}
		// ��k��Ͱ�ǿյģ�����ߵ�Ͱ
		for (size_t i = k + 1; i < NPAGES; i++)
		{
			if (!_spanLists[i].empty())
			{
				Span* nspan = _spanLists[i].pop_front();
				// �зֳ�kҳ��n - kҳ
				// Span* kspan = new Span;
				Span* kspan = _spanPool.New();
				// ��nspan��ͷ����һ��kҳspan
				kspan->_pageId = nspan->_pageId;// ҳ��
				kspan->_n = k;// ҳ��
				nspan->_pageId += k;
				nspan->_n -= k;
				// ��ʣ�µ�ҳ�ٹ�����
				_spanLists[nspan->_n].push_front(nspan);
				// �洢nspan����βҳ����span��ӳ���ϵ������page cache�ϲ�����
				//_idSpan[nspan->_pageId] = nspan;
				//_idSpan[nspan->_pageId + nspan->_n - 1] = nspan;
				_idSpan.set(nspan->_pageId, nspan);
				_idSpan.set(nspan->_pageId + nspan->_n - 1, nspan);
				// ����id �� span��ӳ���ϵ
				for (PAGE_ID i = 0; i < kspan->_n; i++)
				{
					//_idSpan[kspan->_pageId + i] = kspan;
					_idSpan.set(kspan->_pageId + i, kspan);
				}
				return kspan;
			}
		}
		// ����Ҳû��span
		// ���Ҫ128ҳspan
		// Span* spanfromheap = new Span;
		Span* spanfromheap = _spanPool.New();
		void* ptr = SystemAlloc(NPAGES - 1);
		// ��ַתҳ��
		spanfromheap->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		spanfromheap->_n = NPAGES - 1;
		_spanLists[NPAGES - 1].push_front(spanfromheap);
		// ���˴��span��ݹ�����Լ�
		return NewSpan(k);
	}
}


Span* PageCache::MapObjToSpan(void* obj)
{
	// ͨ����ַ��ҳ��
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
	assert(it != nullptr);// û���ҵ�
	return it;
}

// central cache�黹span��page cache
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ����128ҳ
	if (span->_n > NPAGES - 1)
	{
		void* addr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(addr);
		// delete span;
		_spanPool.Delete(span);
	}
	else
	{
		// ��ǰ��ҳ���Ժϲ�
		// ��ǰ�ϲ�
		while (true)
		{
			PAGE_ID preId = span->_pageId - 1;
			// ��ǰ���ҳ��
			/*if (!_idSpan.count(preId))
			{
				break;
			}
			Span* preSpan = _idSpan[preId];*/
			auto it = (Span*)_idSpan.get(preId);
			if (it == nullptr) break;
			Span* preSpan = it;
			// ǰ���ҳ�ű�ʹ��
			if (preSpan->_isUse == true)
			{
				break;
			}
			// span������128ҳ
			if (preSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			// �ϲ�
			span->_pageId = preSpan->_pageId;
			span->_n += preSpan->_n;
			// ��preSpan�Ӷ�Ӧ��˫������ɾ��
			_spanLists[preSpan->_n].erase(preSpan);
			// �����ͷ�������������ɾ�������preSpan�Ľṹ��
			// delete preSpan;
			_spanPool.Delete(preSpan);
		}
		// ���ϲ�
		while (true)
		{
			PAGE_ID nextId = span->_pageId + span->_n;
			// �޺�ߵ�ҳ��
			/*if (!_idSpan.count(nextId))
			{
				break;
			}
			Span* nextSpan = _idSpan[nextId];*/
			auto it = (Span*)_idSpan.get(nextId);
			if (it == nullptr) break;
			Span* nextSpan = it;
			// �����ҳ�ű�ʹ��
			if (nextSpan->_isUse == true)
			{
				break;
			}
			// span������128ҳ
			if (nextSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			// �ϲ�
			// ��ʼҳ�Ų��ñ�
			span->_n += nextSpan->_n;
			// ��nextSpan�Ӷ�Ӧ��˫������ɾ��
			_spanLists[nextSpan->_n].erase(nextSpan);
			// delete nextSpan;
			_spanPool.Delete(nextSpan);
		}

		// ��span������
		_spanLists[span->_n].push_front(span);
		span->_isUse = false;
		// ����βid��span��������������������span�ϲ�
		//_idSpan[span->_pageId] = span;
		//_idSpan[span->_pageId + span->_n - 1] = span;
		_idSpan.set(span->_pageId, span);
		_idSpan.set(span->_pageId + span->_n - 1, span);
	}
}