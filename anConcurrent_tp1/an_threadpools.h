#pragma once

#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <algorithm>
#include <memory>
#include <queue>
#include <vector>
#include <stdexcept>


class an_threadpools{
public:
	explicit an_threadpools(std::size_t threads);
	an_threadpools();

	~an_threadpools();

	template<typename F, typename... Args>
	auto commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type>;


	//
	an_threadpools(const an_threadpools&) = delete;
	an_threadpools operator=(const an_threadpools&) = delete;
	an_threadpools(an_threadpools&&) = delete;
	an_threadpools operator=(an_threadpools&&) = delete;
private:
	static inline std::size_t get_thread_nums() {
		return (std::thread::hardware_concurrency() * 2 - 1);
	}
private:
	std::vector<std::thread> thread_pools_;
	std::queue<std::function<void()> > task_queue_;
	std::mutex queue_mtx_;
	std::condition_variable cond_;
	volatile std::atomic_bool stop_;

};

an_threadpools::an_threadpools(std::size_t threads) :stop_(false) {
	std::size_t system_nums = an_threadpools::get_thread_nums();
	std::size_t workers = threads > system_nums ? system_nums : threads;

	for (std::size_t i = 0; i < system_nums; ++i) {
		thread_pools_.emplace_back(
			[this]() {
			while (true) {
				std::function<void()> task;

				{
					std::unique_lock<std::mutex> lk(this->queue_mtx_);

					this->cond_.wait(lk, [this] { return this->stop_ || !this->task_queue_.empty(); });

					if (this->stop_ && this->task_queue_.empty()) {
						return;
					}

					task = std::move(this->task_queue_.front());
					this->task_queue_.pop();
				}

				task();

			}

		}
		);

	}

}

template<typename F, typename... Args>
auto an_threadpools::commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type> {
	using return_type = typename std::result_of<F(Args...)>::type;

	///关键点：1、利用std::bind 将实际功能函数及参数 生成task可调用对象; 2、通过std::shared_ptr 管理task可调用对象//
	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	///获取 task 返回值future//
	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lk(this->queue_mtx_);

		if (this->stop_) {
			throw std::runtime_error("FATAL:commit_task on stopped an_threadpools!");
		}

		///生成调用task 调用对象的 std::function<void()> 的thread queue item
		this->task_queue_.emplace([task]()->void {(*task)(); }); ///== this->task_queue_.emplace([task]() {(*task)(); });//
	}

	this->cond_.notify_one();

	return res;
}

an_threadpools::an_threadpools() : an_threadpools(std::thread::hardware_concurrency()) {

}

an_threadpools::~an_threadpools() {
	{
		std::unique_lock<std::mutex> lk(this->queue_mtx_);
		stop_ = true;
	}

	this->cond_.notify_all();

	for (auto& worker : this->thread_pools_) {
		worker.join();
	}
}

