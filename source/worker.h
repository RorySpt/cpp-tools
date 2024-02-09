#pragma once
#include <future>


class worker
{
	void run(const std::stop_token& stop_token)
	{
		while (true)
		{
			std::vector<std::move_only_function<void()>> do_tasks;
			{
				std::unique_lock lock(task_mutex);

				bool stop_requested = false;
				task_cv.wait(lock, [&]()->bool
					{
						return ((stop_requested = stop_token.stop_requested())) || !tasks.empty();
					});
				if (stop_requested) return;
				tasks.swap(do_tasks);

			}
			for (auto& task : do_tasks)
			{
				task();
			}
			std::unique_lock lock(task_done_mutex);
			task_done_cv.notify_one();
		}

	}
public:
	worker()
	{
		thread = std::jthread([&](const std::stop_token& stop_token)
			{
				run(stop_token);
			});
	}
	//~worker() { println("worker done."); }
	

	template<typename F, typename ...Args> requires std::invocable<F, Args...>
	auto add_task(F&& task, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
	{
		using R = std::invoke_result_t<F, Args...>;
		std::lock_guard guard(task_mutex);
		//std::promise<R> promise;
		auto package = std::packaged_task<R(Args...)>(std::forward<F>(task));
		auto future = package.get_future();
		tasks.emplace_back([=, package = std::move(package)]()mutable
			{
				if constexpr (sizeof...(Args) > 0)
					package(args...);
				else
					package();
			});
		task_cv.notify_one();
		return future;
	}
	//template<typename F> requires std::invocable<F>
	//auto add_task(F&& task) -> std::future<std::invoke_result_t<F>>
	//{
	//	using R = std::invoke_result_t<F>;
	//	std::lock_guard guard(task_mutex);
	//	//std::promise<R> promise;
	//	auto package = std::packaged_task<R()>(task);
	//	auto future = package.get_future();
	//	tasks.emplace_back([package = std::move(package)]()mutable
	//		{
	//			package();
	//		});
	//	task_cv.notify_one();
	//	return future;
	//}

	void stop_requested()
	{
		thread.request_stop();
		task_cv.notify_one();
		//thread.join();
	}
	void wait_for_all_done()
	{
		std::unique_lock lock(task_done_mutex);
		task_done_cv.wait(lock, [&]()->bool
			{
				return tasks.empty();
			});
	}
	std::atomic_int64_t work_count;
	std::condition_variable task_done_cv;
	std::condition_variable task_cv;
	std::mutex task_done_mutex;
	std::mutex task_mutex;
	std::vector<std::move_only_function<void()>> tasks;
	std::jthread thread;
};
