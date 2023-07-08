#include "ConcurrentAlloc.h"
#include "Common.h"

void ThreadAlloc1()
{
	for (size_t i = 0; i < 5; i++)
	{
		void* ptr = ConcurrentAlloc(8);
		cout << ptr << endl;
	}
}

void ThreadAlloc2()
{
	for (size_t i = 0; i < 5; i++)
	{
		void* ptr = ConcurrentAlloc(7);
		cout << ptr << endl;
	}
}

void TLSTest()
{
	std::thread t1(ThreadAlloc1);
	t1.join();
	std::thread t2(ThreadAlloc1);
	t2.join();
}


void TestAlloc()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(7);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(8);
	void* p5 = ConcurrentAlloc(5);
	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
}

void TestAlloc2()
{
	for (size_t i = 0; i < 1024; i++)
	{
		void* p1 = ConcurrentAlloc(8);
		cout << p1 << endl;
	}
	void* p2 = ConcurrentAlloc(5);
	cout << p2 << endl;
}


void Thread1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
}

void Thread2()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
}

void DelTest()
{
	std::thread t1(Thread1);
	t1.join();
	std::thread t2(Thread2);
	t2.join();
}

void BigAlloc()
{
	void* p1 = ConcurrentAlloc(257 * 1024);// 257kb
	ConcurrentFree(p1);
	void* p2 = ConcurrentAlloc(129 * 8 * 1024);// 19ҳ
	ConcurrentFree(p2);
}

void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 1024; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}
	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}
void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 1024; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}
	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}

void MulTest()
{
	std::thread t1(MultiThreadAlloc1);
	t1.join();
	std::thread t2(MultiThreadAlloc2);
	t2.join();
}


//int main()
//{
//	MulTest();
//	return 0;
//}