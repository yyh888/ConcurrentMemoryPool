#pragma once
#include "Common.h"

class CentralCache
{
public:
	static CentralCache& GetSingleton()
	{
		return _cSingle;
	}

	// �����Ļ����л�ȡһ�������Ķ����threadcache
	size_t FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size);
	// ��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t size);
	// ��һ��������thread cache���յĶ���ҵ���Ӧ��span
	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanList[NFREELISTS];
private:
	// ����˽��+������
	CentralCache(){}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	// ����
	static CentralCache _cSingle;
};