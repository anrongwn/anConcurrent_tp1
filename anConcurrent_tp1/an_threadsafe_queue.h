#pragma once

#include <functional>
#include <thread>
#include <future>
#include <algorithm>
#include <utility>
#include <chrono>

#include <mutex>
#include <condition_variable>
#include <atomic>

#include <queue>

template<typename T>
class an_threadsafe_queue {
private:
	mutable std::mutex mut_;
	std::queue<T> data_queue_;
	std::condition_variable data_cond_;

public:
	an_threadsafe_queue() {

	}

	bool empty() const {
		std::lock_guard<std::mutex> lk(mut_);
		return data_queue_.empty();
	}

	void push(T data) {
		std::lock_guard<std::mutex> lock(mut_);
		data_queue_.push(std::move(data));

		data_cond_.notify_one();
	}

	void wait_and_pop(T &value) {
		std::unique_lock<std::mutex> lk(mut_);

		data_cond_.wait(lk, [this]() {return !data_queue_.empty(); });

		value = std::move(data_queue_.front());

		data_queue_.pop();
	}

	std::shared_ptr<T> wait_and_pop() {
		std::unique_lock<std::mutex> lk(mut_);
		data_cond_.wait(lk, [this] {return !data_queue_.empty(); });

		std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue_.front())));
		data_queue_.pop();

		return res;
	}

	bool try_pop(T& value) {
		std::lock_guard<std::mutex> lk(mut_);
		if (data_queue_.empty())
			return false;

		value = std::move(data_queue_.front());
		data_queue_.pop();

		return true;
	}

	std::shared_ptr<T> try_pop() {
		std::lock_guard<std::mutex> lk(mut_);
		if (data_queue_.empty())
			return std::shared_ptr<T>();


		std::shared_ptr<T> res(
			std::make_shared<T>(std::move(data_queue_.front())));
		data_queue_.pop();

		return res;
	}
};


template<typename T>
class lock_free_stack {
private:
	struct node {
		std::shared_ptr<T> data;  // 1 指针获取数据
		node* next;

		node(T const& data_) :
			data(std::make_shared<T>(data_))  // 2 让std::shared_ptr指向新分配出来的T
		{
			next = nullptr;
		}
	};

	std::atomic<node*> head;
	
public:
	void push(T const& data) {
		node* const new_node = new node(data);
		new_node->next = head.load();

		while (!head.compare_exchange_weak(new_node->next, new_node));
	}
	std::shared_ptr<T> pop()
	{
		node* old_head = head.load();
		while (old_head && // 3 在解引用前检查old_head是否为空指针
			!head.compare_exchange_weak(old_head, old_head->next));
		return old_head ? old_head->data : std::shared_ptr<T>();  // 4
	}
};