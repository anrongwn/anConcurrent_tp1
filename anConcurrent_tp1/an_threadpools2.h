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
#include "../deps/concurrentqueue/concurrentqueue.h"

//non thread-safe 
class an_threadpools2 {
public:
	explicit an_threadpools2(std::size_t count);
	an_threadpools2();

	~an_threadpools2();

	template<typename F, typename... Args>
	auto commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type>;

	//
	an_threadpools2(const an_threadpools2&) = delete;
	an_threadpools2& operator=(const an_threadpools2&) = delete;
	an_threadpools2(an_threadpools2&&) = delete;
	an_threadpools2& operator=(an_threadpools2&&) = delete;
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

	//non block queue
	moodycamel::ConcurrentQueue<std::function<void()> > task_queue_;

	volatile std::atomic_bool stop_;

};

//non thread-safe 
an_threadpools2::an_threadpools2(std::size_t count) :stop_(false) {
	std::size_t system_nums = an_threadpools2::get_thread_nums();
	std::size_t workers = count > system_nums ? system_nums : count;

	for (std::size_t i = 0; i < workers; ++i) {
		thread_pools_.emplace_back(
			[this]() {
			while (!this->stop_) {
				std::function<void()> task;

				if (this->task_queue_.try_dequeue(task)) {
					task();
				}
				else {
					//std::this_thread::yield(); //cpu busy
					std::this_thread::sleep_for(std::chrono::microseconds(10));
				}

				/*
				{
					
					std::unique_lock<std::mutex> lk(this->queue_mtx_);

					///��stop=true ���� task_queue_ ���в�Ϊ��ʱ���ɽ����ǰblocking������queue_mtx_;������ͷ�queue_mtx_����ǰblock
					this->cond_.wait(lk, [this] { return this->stop_ || !this->task_queue_.empty(); });

					///�Ƿ��˳��߳�
					if (this->stop_ && this->task_queue_.empty()) {
						return;
					}

					///ȡ std::function<void> �߳�item
					task = std::move(this->task_queue_.front());
					this->task_queue_.pop();
				}
				

				///���� std::function<void> �̶߳���ִ��std::bind ��std::packaged_task 
				task();
				*/

			}

		}
		);

	}

}

template<typename F, typename... Args>
auto an_threadpools2::commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type> {
	using return_type = typename std::result_of<F(Args...)>::type;

	///�ؼ��㣺1������std::bind ��ʵ�ʹ��ܺ��������� ����task�ɵ��ö���; 2��ͨ��std::shared_ptr ����task�ɵ��ö���//
	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	///��ȡ task ����ֵfuture//
	std::future<return_type> res = task->get_future();

	this->task_queue_.enqueue([task]()->void {(*task)(); });

	/*
	{
		std::unique_lock<std::mutex> lk(this->queue_mtx_);

		if (this->stop_) {
			throw std::runtime_error("FATAL:commit_task on stopped an_threadpools2!");
		}

		///���ɵ���task ���ö���� std::function<void()> ��thread queue item
		this->task_queue_.emplace([task]()->void {(*task)(); }); ///== this->task_queue_.emplace([task]() {(*task)(); });//
	}
	this->cond_.notify_one();
	*/

	return res;
}

an_threadpools2::an_threadpools2() : an_threadpools2(std::thread::hardware_concurrency()) {

}

an_threadpools2::~an_threadpools2() {
	{
		//std::unique_lock<std::mutex> lk(this->queue_mtx_);
		stop_ = true;
	}

	//this->cond_.notify_all();

	for (auto& worker : this->thread_pools_) {
		worker.join();
	}
}
