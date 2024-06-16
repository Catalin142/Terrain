#include "Instrumentor.h"

void BenchmarkTimer::Start()
{
	m_Time = std::chrono::steady_clock::now();
}

void BenchmarkTimer::Stop()
{
	Shutdown();
}

void BenchmarkTimer::Shutdown()
{
	auto end = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch() -
		std::chrono::time_point_cast<std::chrono::microseconds>(m_Time).time_since_epoch();

	Instrumentor::Get().Write(m_FunName, (float)elapsed.count());
}

//#if DEBUG == 1
void Instrumentor::beginTimer(const std::string& name)
{
	if (m_Timers.find(name) == m_Timers.end())
		m_Timers[name] = BenchmarkTimer(name.c_str());
	m_Timers[name].Start();
}

void Instrumentor::endTimer(const std::string& name)
{
	if (m_Timers.find(name) == m_Timers.end())
		return;
	m_Timers[name].Stop();
}

float Instrumentor::getTime(const std::string& name)
{
	if (m_BenchmarkTime.find(name) == m_BenchmarkTime.end())
		return 0.0f;
	return m_BenchmarkTime[name];
}
//#endif