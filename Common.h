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

static const size_t MAX_BYTES = 256 * 1024;// 最大申请256KB
static const size_t NFREELISTS = 208;// 最大208个自由链表桶
static const size_t NPAGES = 129;// page cache最大桶数
static const size_t PAGE_SHIFT = 13;// 右移13位，8k

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

// 直接从堆上申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << 13), MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

// 直接将内存还给堆
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	//linux下sbrk unmmap等
#endif
}

// 管理多个页大块内存
struct Span
{
	PAGE_ID _pageId = 0;// 大块内存起始页号
	size_t _n = 0;// 页数

	Span* _prev = nullptr;// 双向链表结构
	Span* _next = nullptr;

	size_t _useCnt = 0;// 分配给thread cache的计数
	void* _freeList = nullptr;// 切好小内存的链表

	bool _isUse = false;// 该span是否被使用

	size_t _objSize = 0;// 切分的小对象的大小
};

static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

// 自由链表
class FreeList
{
public:
	// 将释放的对象头插到自由链表
	void push_front(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		_size++;
	}

	// 从自由链表头部获取一个对象
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

	// 头插入多个对象(n个)到自由链表
	void push_range(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	// 头删n个对象，用输出型参数返回
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
	size_t _size = 0;// 个数
};

// 管理对齐和映射规则
class SizeRule
{
public:
	static inline size_t _RoundUp(size_t bytes, size_t alignNum/*对齐数*/)
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
			// 以页为单位进行对齐
			return _RoundUp(bytes, 1 << PAGE_SHIFT/*8k*/);
		}
		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t alignShift)
	{
		return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
	}

	// 计算映射的桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// 每个区间的桶数
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

	// threadchahe一次从central cache获取多少个对象
	static size_t NumMoveSize(size_t size/*单个对象大小*/)
	{
		assert(size > 0);
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// central cache一次向page cache获取多少页
	static size_t NumMovePage(size_t bytes)
	{
		// 申请对象的个数上限
		size_t num = NumMoveSize(bytes);
		// num个size大小的对象所需的字节数
		size_t npage = num * bytes;
		// 算出页数
		npage >>= PAGE_SHIFT;
		if (npage == 0)
		{
			npage = 1;
		}
		return npage;
	}
};


// 双向链表
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
	std::mutex _mtx;// 桶锁
};