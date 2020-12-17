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
	explicit an_threadpools(std::size_t count);
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
	volatile bool stop_;

};

an_threadpools::an_threadpools(std::size_t count) :stop_(false) {
	std::size_t system_nums = an_threadpools::get_thread_nums();
	std::size_t workers = count > system_nums ? system_nums : count;

	for (std::size_t i = 0; i < system_nums; ++i) {
		thread_pools_.emplace_back(
			[this]() {
			while (true) {
				std::function<void()> task;

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

			}

		}
		);

	}

}

template<typename F, typename... Args>
auto an_threadpools::commit_task(F&& f, Args&&...args)->std::future<typename std::result_of<F(Args...)>::type> {
	using return_type = typename std::result_of<F(Args...)>::type;

	///�ؼ��㣺1������std::bind ��ʵ�ʹ��ܺ��������� ����task�ɵ��ö���; 2��ͨ��std::shared_ptr ����task�ɵ��ö���//
	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	///��ȡ task ����ֵfuture//
	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lk(this->queue_mtx_);

		if (this->stop_) {
			throw std::runtime_error("FATAL:commit_task on stopped an_threadpools!");
		}

		///���ɵ���task ���ö���� std::function<void()> ��thread queue item
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

