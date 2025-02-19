#pragma once

#include <unordered_map>
#include <string>

class SimpleVulkanMemoryTracker
{
public:
	static SimpleVulkanMemoryTracker* Get()
	{
		if (m_Instance == nullptr)
			m_Instance = new SimpleVulkanMemoryTracker{};
		return m_Instance;
	}

	void Track(const std::string& field) { m_CurrentField = field; }
	void Flush(const std::string& field) { m_AllocatedMemory[field] = 0; }
	void Stop() { m_CurrentField = ""; }

	void Track(uint32_t size)
	{ 
		if (m_CurrentField.empty())
			return;

		if (m_AllocatedMemory.find(m_CurrentField) == m_AllocatedMemory.end())
			m_AllocatedMemory[m_CurrentField] = 0;

		m_AllocatedMemory[m_CurrentField] += size;
	}

	uint32_t getAllocatedMemory(const std::string& field) { return m_AllocatedMemory[field]; }

private:
	SimpleVulkanMemoryTracker() = default;
	~SimpleVulkanMemoryTracker() = default;

private:
	static inline SimpleVulkanMemoryTracker* m_Instance;

	std::unordered_map<std::string, uint32_t> m_AllocatedMemory;
	std::string m_CurrentField;
};


#if DEBUG == 1
#define TRACK_VK_MEMORY(var) SimpleVulkanMemoryTracker::Get()->Track(var);
#elif
#define TRACK_VK_MEMORY(var)
#endif
