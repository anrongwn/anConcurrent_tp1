#pragma once

#include <thread>
//#include <mutex>
#include <future>
//#include <condition_variable>
#include <atomic>
#include <functional>
#include <algorithm>
#include <memory>
#include <queue>
#include <vector>
#include <stdexcept>
#include "../deps/concurrentqueue/blockingconcurrentqueue.h"

//non thread-safe 
class an_threadpools3 {
public:
	explicit an_threadpools3(std::size_t count);
	an_threadpools3();

	~an_threadpools3();

	template<typename F, typename... Args>
	auto commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type>;

	//
	an_threadpools3(const an_threadpools3&) = delete;
	an_threadpools3& operator=(const an_threadpools3&) = delete;
	an_threadpools3(an_threadpools3&&) = delete;
	an_threadpools3& operator=(an_threadpools3&&) = delete;
private:
	static inline std::size_t get_thread_nums() {
		//return (std::thread::hardware_concurrency() * 2 - 1);
		return (std::thread::hardware_concurrency() + 1);
	}

private:
	std::vector<std::thread> thread_pools_;
	/*std::queue<std::function<void()> > task_queue_;
	std::mutex queue_mtx_;
	std::condition_variable cond_;
	*/

	//block queue
	moodycamel::BlockingConcurrentQueue<std::function<void()> > task_queue_;

	volatile std::atomic_bool stop_;

};

//non thread-safe 
an_threadpools3::an_threadpools3(std::size_t count) :stop_(false) {
	std::size_t system_nums = an_threadpools3::get_thread_nums();
	std::size_t workers = count > system_nums ? system_nums : count;

	for (std::size_t i = 0; i < system_nums; ++i) {
		thread_pools_.emplace_back(
			[this]() {
			while (!this->stop_) {
				std::function<void()> task;

				/*this->task_queue_.wait_dequeue(task);
				task();*/

				//较吃cpu 
				if (this->task_queue_.wait_dequeue_timed(task, std::chrono::microseconds(1))) {
					task();
				}
				else {
					std::this_thread::sleep_for(std::chrono::microseconds(10));
				}
				


				//if (this->task_queue_.try_dequeue(task)) {
				//	task();
				//}
				//else {
				//	//std::this_thread::yield(); //cpu busy
				//	std::this_thread::sleep_for(std::chrono::microseconds(10));
				//}

			

			}

		}
		);

	}

}

template<typename F, typename... Args>
auto an_threadpools3::commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type> {
	using return_type = typename std::result_of<F(Args...)>::type;

	///关键点：1、利用std::bind 将实际功能函数及参数 生成task可调用对象; 2、通过std::shared_ptr 管理task可调用对象//
	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	///获取 task 返回值future//
	std::future<return_type> res = task->get_future();

	this->task_queue_.enqueue([task]()->void {(*task)(); });

	/*
	{
		std::unique_lock<std::mutex> lk(this->queue_mtx_);

		if (this->stop_) {
			throw std::runtime_error("FATAL:commit_task on stopped an_threadpools3!");
		}

		///生成调用task 调用对象的 std::function<void()> 的thread queue item
		this->task_queue_.emplace([task]()->void {(*task)(); }); ///== this->task_queue_.emplace([task]() {(*task)(); });//
	}
	this->cond_.notify_one();
	*/

	return res;
}

an_threadpools3::an_threadpools3() : an_threadpools3(std::thread::hardware_concurrency()) {

}

an_threadpools3::~an_threadpools3() {
	{
		//std::unique_lock<std::mutex> lk(this->queue_mtx_);
		stop_ = true;
	}

	//this->cond_.notify_all();

	for (auto& worker : this->thread_pools_) {
		worker.join();
	}
}
