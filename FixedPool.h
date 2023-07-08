#pragma once
#include "Common.h"
#include <iostream>
#include <vector>


//	// window
//#ifdef WIN32
//#include <Windows.h>
//#else
//	//linux(brk, mmap)
//#endif
//
//// 直接从堆上申请空间
//inline static void* SystemAlloc(size_t kpage)
//{
//#ifdef _WIN32
//	void* ptr = VirtualAlloc(0, kpage * (1 << 13), MEM_COMMIT | MEM_RESERVE,
//		PAGE_READWRITE);
//#else
//	// linux下brk mmap等
//#endif
//	if (ptr == nullptr)
//		throw std::bad_alloc();
//	return ptr;
//}

using std::cout;
using std::endl;


// 定长内存池
template <class T>
class FixedPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		// 先用归还的内存
		if (_freeList)
		{
			void* next = NextObj(_freeList);
			obj = (T*)_freeList;
			_freeList = next;
			return obj;
		}
		// 剩余空间不足一个对象大小
		if (_remainBytes < sizeof(T))
		{
			_remainBytes = 128 * 1024;
			_memory = (char*)SystemAlloc(_remainBytes >> 13);
			if (_memory == nullptr)
			{
				throw std::bad_alloc();
			}
		}
		obj = (T*)_memory;
		// 不足一个指针，就给一个指针大小，方便链接
		size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
		_memory += sizeof(T);
		_remainBytes -= sizeof(T);
		// 定位new初始化对象
		new (obj)T;
		return obj;
	}

	void Delete(T* obj)
	{
		// 显示调用析构
		obj->~T();
		if (_freeList == nullptr)
		{
			_freeList = obj;
			NextObj(obj) = nullptr;
		}
		else
		{
			// 头插
			NextObj(obj) = _freeList;
			_freeList = obj;
		}
	}
private:
	char* _memory = nullptr;// 大块内存
	void* _freeList = nullptr;// 归还的内存
	size_t _remainBytes = 0;// 剩余空间大小
};

//struct A
//{
//	int _a = 0;
//	int _b = 1;
//	double* _p1 = nullptr;
//	double* _p2 = nullptr;
//};

//void TestFixedPool()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 3;
//	// 每轮申请释放多少次
//	const size_t N = 1000000;
//	size_t begin1 = clock();
//	std::vector<A*> v1;
//	v1.reserve(N);
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new A);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i];
//		}
//		v1.clear();
//	}
//	size_t end1 = clock();
//	FixedPool<A> TNPool;
//	size_t begin2 = clock();
//	std::vector<A*> v2;
//	v2.reserve(N);
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < 100000; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//	cout << "new time:" << end1 - begin1 << endl;
//	cout << "Fixed pool time:" << end2 - begin2 << endl;
//}