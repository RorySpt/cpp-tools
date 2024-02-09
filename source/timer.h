#pragma once
#include "print.h"
#include <chrono>
class timer
{
	using clock = std::chrono::high_resolution_clock;

public:
	timer() { reset(); }

	void reset()
	{
		start_tp = clock::now();
	}

	auto peek() const
	{
		const auto peek_tp = clock::now() - start_tp;
		return std::chrono::duration<double>(peek_tp);
	}
	auto seek()
	{
		auto now = clock::now();
		const auto peek_tp = now - std::exchange(start_tp, now);
		return std::chrono::duration<double>(peek_tp);
	}
	void peek_print() const
	{
		println("cost: {}", peek());
	}
	void seek_print()
	{
		println("cost: {}", seek());
	}
private:
	clock::time_point start_tp = clock::now();
};
