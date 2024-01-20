#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <thread>
#include <chrono>

#define BENCHMARK 1

class Instrumentor
{
	friend class BenchmarkTimer;

public:
	void beginSession(const std::string& sessioname, const std::string& filepath = "profile.json")
	{
		if (m_CurrentSession != sessioname)
		{
			m_jsonFile.open(filepath);
			m_CurrentSession = sessioname;
		}
		m_jsonFile << "{\"otherData\": {},\"traceEvents\":[{}\n";
		m_jsonFile.flush();
	}

	void endSession()
	{
		m_jsonFile << "]}\n";
		m_jsonFile.flush();
		m_jsonFile.close();
		m_ProfileCount = 0;
	}

	float getTime(const std::string& name)
	{
		if (m_BenchmarkTime.find(name) == m_BenchmarkTime.end())
			return 0.0f;
		return m_BenchmarkTime[name];
	}

	static Instrumentor& Get()
	{
		static Instrumentor instance;
		return instance;
	}

private:
	Instrumentor() : m_ProfileCount(0) { }
	~Instrumentor() = default;

	void Write(const char* name, float duration, std::chrono::duration<double, std::micro> start, std::thread::id threadID) 
	{
		//std::stringstream json;
		//
		//json << std::setprecision(3) << std::fixed;
		//json << "\t,{\n";
		//json << "\t\"cat\":\"function\",\n";
		//json << "\t\"dur\":" << duration << ", \n";
		//json << "\t\"name\":\"" << name << "\",\n";
		//json << "\t\"ph\":\"X\",\n";
		//json << "\t\"pid\":1,\n";
		//json << "\t\"tid\":" << threadID << ",\n";
		//json << "\t\"ts\":" << start.count() << "\n";
		//json << "\t}";
		//
		//m_jsonFile << json.str();
		//
		//m_jsonFile.flush();
		if (m_BenchmarkTime.find(name) == m_BenchmarkTime.end())
			m_BenchmarkTime[name] = duration / 1000.0f;
		m_BenchmarkTime[name] += duration / 1000.0f;
		m_BenchmarkTime[name] /= 2.0;
	}

private:
	std::string m_CurrentSession;
	std::ofstream m_jsonFile;
	int m_ProfileCount;

	std::unordered_map<std::string, float> m_BenchmarkTime;
};

class BenchmarkTimer
{
public:
	BenchmarkTimer(const char* functionName) : m_FunName(functionName)
	{
		m_Time = std::chrono::steady_clock::now();
	}

	~BenchmarkTimer()
	{
		if (!m_Stopped)
			Shutdown();
	}

	void Shutdown()
	{
		auto end = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch() -
			std::chrono::time_point_cast<std::chrono::microseconds>(m_Time).time_since_epoch();

		auto Start = std::chrono::duration<double, std::micro>{ m_Time.time_since_epoch() };

		Instrumentor::Get().Write(m_FunName, (float)elapsed.count(), Start, std::this_thread::get_id());

		m_Stopped = true;
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> m_Time;
	const char* m_FunName;
	bool m_Stopped = false;
};

#if DEBUG == 1 && BENCHMARK == 1
#define START_SCOPE_PROFILE(name) BenchmarkTimer time##__LINE__(name)
#define BEGIN_SESSION(name, file) Instrumentor::Get().beginSession(name, file)
#define END_SESSION 			  Instrumentor::Get().endSession()

#else
#define START_SCOPE_PROFILE(name)
#define BEGIN_SESSION(name, file)
#define END_SESSION
#endif