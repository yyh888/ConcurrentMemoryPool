#pragma once
#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include <thread>
#include <mutex>
#include <algorithm>
#include <unordered_map>
#include <memory>

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024;// �������256KB
static const size_t NFREELISTS = 208;// ���208����������Ͱ
static const size_t NPAGES = 129;// page cache���Ͱ��
static const size_t PAGE_SHIFT = 13;// ����13λ��8k

// window
#ifdef WIN32
#include <Windows.h>
#else
	//linux(brk, mmap)
#endif

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
// linux
#endif

// ֱ�ӴӶ�������ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << 13), MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

// ֱ�ӽ��ڴ滹����
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	//linux��sbrk unmmap��
#endif
}

// ������ҳ����ڴ�
struct Span
{
	PAGE_ID _pageId = 0;// ����ڴ���ʼҳ��
	size_t _n = 0;// ҳ��

	Span* _prev = nullptr;// ˫������ṹ
	Span* _next = nullptr;

	size_t _useCnt = 0;// �����thread cache�ļ���
	void* _freeList = nullptr;// �к�С�ڴ������

	bool _isUse = false;// ��span�Ƿ�ʹ��

	size_t _objSize = 0;// �зֵ�С����Ĵ�С
};

static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

// ��������
class FreeList
{
public:
	// ���ͷŵĶ���ͷ�嵽��������
	void push_front(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		_size++;
	}

	// ����������ͷ����ȡһ������
	void* pop_front()
	{
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		_size--;
		return obj;
	}

	bool empty()
	{
		return _freeList == nullptr;
	}

	// ͷ����������(n��)����������
	void push_range(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	// ͷɾn������������Ͳ�������
	void pop_range(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = start;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	size_t size()
	{
		return _size;
	}

	size_t& max_size()
	{
		return _maxSize;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;// ����
};

// ��������ӳ�����
class SizeRule
{
public:
	static inline size_t _RoundUp(size_t bytes, size_t alignNum/*������*/)
	{
		return ((bytes + alignNum - 1) & ~(alignNum - 1));
	}

	static inline size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8 * 1024);
		}
		else
		{
			// ��ҳΪ��λ���ж���
			return _RoundUp(bytes, 1 << PAGE_SHIFT/*8k*/);
		}
		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t alignShift)
	{
		return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
	}

	// ����ӳ���Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// ÿ�������Ͱ��
		int Group[4] = { 16, 56, 56, 56 };
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + Group[0];
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 7) + Group[0] + Group[1];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 10) + Group[0] + Group[1] + Group[2];
		}
		else if (bytes <= 256 * 1024)
		{
			return _Index(bytes - 64 * 1024, 13) + Group[0] + Group[1] + Group[2] + Group[3];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	// threadchaheһ�δ�central cache��ȡ���ٸ�����
	static size_t NumMoveSize(size_t size/*���������С*/)
	{
		assert(size > 0);
		// [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
		// С����һ���������޸�
		// С����һ���������޵�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// central cacheһ����page cache��ȡ����ҳ
	static size_t NumMovePage(size_t bytes)
	{
		// �������ĸ�������
		size_t num = NumMoveSize(bytes);
		// num��size��С�Ķ���������ֽ���
		size_t npage = num * bytes;
		// ���ҳ��
		npage >>= PAGE_SHIFT;
		if (npage == 0)
		{
			npage = 1;
		}
		return npage;
	}
};


// ˫������
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_prev = _head;
		_head->_next = _head;
	}

	Span* begin()
	{
		return _head->_next;
	}

	Span* end()
	{
		return _head;
	}

	void insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);
		newSpan->_prev = pos->_prev;
		pos->_prev->_next = newSpan;
		pos->_prev = newSpan;
		newSpan->_next = pos;
	}

	bool empty()
	{
		return _head->_next == _head;
	}

	void push_front(Span* span)
	{
		insert(begin(), span);
	}

	Span* pop_front()
	{
		Span* front = begin();
		erase(front);
		return front;
	}

	void erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);
		pos->_prev->_next = pos->_next;
		pos->_next->_prev = pos->_prev;
	}

	void lock()
	{
		_mtx.lock();
	}

	void unlock()
	{
		_mtx.unlock();
	}
private:
	Span* _head;
	std::mutex _mtx;// Ͱ��
};