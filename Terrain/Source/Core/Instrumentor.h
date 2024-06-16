#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <thread>
#include <chrono>

class BenchmarkTimer
{
public:
	BenchmarkTimer() = default;
	BenchmarkTimer(const std::string& functionName) : m_FunName(functionName) {}

	~BenchmarkTimer() {}

	void Start();
	void Stop();
	void Shutdown();

private:
	std::chrono::time_point<std::chrono::steady_clock> m_Time;
	std::string m_FunName = "default";
};

class Instrumentor
{
	friend class BenchmarkTimer;

public:

//#if DEBUG == 1

	void beginTimer(const std::string& name);
	void endTimer(const std::string& name);
	float getTime(const std::string& name);

//#else

	/*void beginTimer(const std::string& name)
	{}

	void endTimer(const std::string& name)
	{}

	float getTime(const std::string& name)
	{	
		return 0.0f;
	}*/

//#endif

	static Instrumentor& Get()
	{
		static Instrumentor instance;
		return instance;
	}

private:
	Instrumentor() = default;
	~Instrumentor() = default;

	void Write(const std::string& name, float duration) 
	{
		m_BenchmarkTime[name] = duration / 1000.0f;
	}

private:
	std::unordered_map<std::string, float> m_BenchmarkTime;
	std::unordered_map<std::string, BenchmarkTimer> m_Timers;
};
