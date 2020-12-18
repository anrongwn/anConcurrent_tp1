// anConcurrent_tp1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <vector>

#include <functional>
#include <thread>
#include <future>
#include <algorithm>
#include <utility>
#include <chrono>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <assert.h>

#include "an_threadsafe_queue.h"
#include "an_threadpools.h"

#include "an_threadpools2.h"


std::future<int> launcher(std::packaged_task<int(int)> &tsk, int arg) {
	if (tsk.valid()) {
		std::future<int> ret = tsk.get_future();

		std::thread(std::move(tsk), arg).detach();
		return ret;
	}
	else {
		return std::future<int>();
	}
}

bool is_prime(int x) {
	for (int i = 2; i < x; ++i) {
		if (x % 2 == 0) {
			return false;
		}
	}

	return true;
}

void do_print_ten(char c, int ms) {
	std::cout << c << "'current thid = " << std::this_thread::get_id() << std::endl;
	for (int i = 0; i < 10; ++i) {
		std::this_thread::sleep_for(std::chrono::microseconds(ms));

		std::cout << c;
	}
}

static std::atomic_flag g_atomic_flag = ATOMIC_FLAG_INIT;
void f1(int n) {

	std::cout << n << "'s current tid : " << std::this_thread::get_id() << std::endl;
	for (int cnt = 0; cnt < 100; ++cnt) {
		while (g_atomic_flag.test_and_set(std::memory_order_acquire))
			;

		std::cout << "output thrend n = " << n << std::endl;

		g_atomic_flag.clear(std::memory_order_release);

	}
}

std::atomic<bool> x, y;
std::atomic<int> z;

void write_x() {
	x.store(true, std::memory_order_seq_cst);
}
void write_y() {
	y.store(true, std::memory_order_seq_cst);
}

void read_x_then_y() {
	while (!x.load(std::memory_order_seq_cst))
		if (y.load(std::memory_order_seq_cst))
			++z;
}
void read_y_then_x() {
	while (!y.load(std::memory_order_seq_cst))
		if (x.load(std::memory_order_seq_cst))
			++z;
}



void dummy_download_test(std::string url) {
	std::this_thread::sleep_for(std::chrono::seconds(3));
	std::cout <<  std::this_thread::get_id() << " complete download: " << url << std::endl;
}

std::string get_user_name_test(int id) {
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return "user_" + std::to_string(id);
}


int main()
{
    std::cout << "Hello anConcurrent tp1!\n";

	/**1
	std::packaged_task<int(int)> tsk([](int x) {return x * 2; });

	std::future<int> fut = launcher(std::ref(tsk), 25);

	std::cout << "x=25,x=2*x, result is = " << fut.get() << std::endl;
	*/

	/**2
	std::future<bool> fut = std::async(is_prime, 444444443);
	std::cout << "checking,please wait";

	std::chrono::milliseconds span(100);
	while (fut.wait_for(span) == std::future_status::timeout)
		std::cout << ".";

	bool x = fut.get();

	std::cout << "\n444444443 " << (x ? "is" : "is not") << " prime.\n";
	*/

	/**3
	std::cout << "with launch::async: \n";

	std::future<void> foo = std::async(std::launch::async, do_print_ten, '*', 100);

	std::future<void> bar = std::async(std::launch::async, do_print_ten, '@', 200);

	foo.get();
	bar.get();
	std::cout << "\n\n";


	std::cout << "with launch::deferred: \n";

	foo = std::async(std::launch::deferred, do_print_ten, '#', 100);

	bar = std::async(std::launch::deferred, do_print_ten, '$', 200);

	foo.get();
	bar.get();
	std::cout << "\n\n";
	*/

	/**4
	std::vector<std::thread> thv;
	for (int n = 0; n < 10; ++n) {
		thv.emplace_back(f1, n);
	}

	for (auto &t : thv) {
		t.join();
	}
	*/

	/**5
	x = false;
	y = false;
	z = 0;

	std::thread a(write_x);
	std::thread b(write_y);
	std::thread c(read_x_then_y);
	std::thread d(read_y_then_x);

	a.join();
	b.join();
	c.join();
	d.join();

	assert(z.load() != 0);
	*/

	std::cout << "max thread num = " << std::thread::hardware_concurrency() << std::endl;
	//lock_free_stack<int> mystack;
	//mystack.push(5);

	/**有锁线程池
	an_threadpools tp;

	//不需等 线程执行结果的
	tp.commit_task(dummy_download_test, "www.126.com/1.png");
	tp.commit_task(dummy_download_test, "www.aliyun.com/2.png");

	//需要等 线程执行结果
	std::vector<std::future<std::string>> vecStr;
	vecStr.emplace_back(tp.commit_task(get_user_name_test, 1000));
	vecStr.emplace_back(tp.commit_task(get_user_name_test, 1001));
	*/

	////异步等
	//std::future<void> res1 = std::async(std::launch::async, [&vecStr]() {
	//	for (auto &&ret : vecStr) {
	//		std::cout << "get user: " << ret.get();
	//	}
	//	std::cout << std::endl;
	//});

	/**无锁线程池**/
	an_threadpools2 tp2;

	//不需等 线程执行结果的
	tp2.commit_task(dummy_download_test, "www.126.com2/1.png");
	tp2.commit_task(dummy_download_test, "www.aliyun.com2/2.png");

	//需要等 线程执行结果
	std::vector<std::future<std::string>> vecStr;
	vecStr.emplace_back(tp2.commit_task(get_user_name_test, 2000));
	vecStr.emplace_back(tp2.commit_task(get_user_name_test, 2001));




	//同步等
	for (auto &&ret : vecStr) {
		std::cout << "get user: " << ret.get();
	}
	std::cout << std::endl;


	//for (;;) {
	//	//std::this_thread::yield();
	//	std::this_thread::sleep_for(std::chrono::microseconds(10));
	//}

	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
