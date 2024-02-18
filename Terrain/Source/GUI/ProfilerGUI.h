#pragma once

#include "imgui/imgui.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

class ProfilerGUI
{
	friend class ProfilerManager;

public:
	ProfilerGUI(const std::string& name, uint32_t sampleCount = 100);
	~ProfilerGUI() = default;
	
	void addProfileValue(const std::string& name, float value, uint32_t color = 0xff0000ff);
	void nextFrame();

private:
	void Render(ImVec2 size);

private:
	std::unordered_map<std::string, std::vector<float>> m_ProfilerValues;
	std::unordered_map<std::string, float> m_ProfilerAverage;
	std::unordered_map<std::string, uint32_t> m_ProfilerColors;

	float m_MaxValue = 0.0;
	std::string m_Name;

	uint32_t m_SampleCount = 100;
	uint32_t m_CurrentSamples = 0;

	uint32_t m_Start = 0;
	uint32_t m_End = 0;

	float m_LineWidth = 1.0f;
};

class ProfilerManager
{
public:
	ProfilerManager(ImVec2 position, float width);

	void addProfiler(const std::string& name, uint32_t sampleCount);
	std::shared_ptr<ProfilerGUI> operator[](const std::string& name);

	void Render();

private:
	ImVec2 m_Position;
	float m_Width;

	std::unordered_map<std::string, std::shared_ptr<ProfilerGUI>> m_Profilers;
};
