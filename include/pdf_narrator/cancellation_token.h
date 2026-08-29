#pragma once

#include <atomic>
#include <cstdint>

class GenerationID {
public:
	uint32_t get() const
	{
		return m_id.load(std::memory_order_relaxed);
	}

	void increment()
	{
		m_id.fetch_add(1, std::memory_order_relaxed);
	}

	bool isStale(uint32_t taskGen) const
	{
		return taskGen != get();
	}

private:
	std::atomic<uint32_t> m_id{ 0 };
};
