#include "ProfilerGUI.h"

#include <cmath>
#include <iostream>
#include <fstream>
#include <format>

#include "Graphics/Vulkan/VulkanDevice.h"

ProfilerGUI::ProfilerGUI(const std::string& name, uint32_t sampleCount) : m_SampleCount(sampleCount), m_Name(name)
{ }

void ProfilerGUI::Flush(const std::string& name)
{
	m_Name = name;

	m_ProfilerValues.clear();
	m_ProfilerAverage.clear();
	m_ProfilerColors.clear();
}

void ProfilerGUI::pushValue(const std::string& name, float value, uint32_t color)
{
	if (value < 0.001)
	{
		if (m_ProfilerValues.find(name) != m_ProfilerValues.end())
		{
			int32_t lastValue = m_End - 1;
			if (lastValue < 0)
				lastValue = m_SampleCount - 1;
			value = m_ProfilerValues[name][lastValue];
		}
		else
			value = 0.001;
	}

	m_MaxValue = std::max(m_MaxValue, value);
	if (m_ProfilerValues.find(name) == m_ProfilerValues.end())
	{
		m_ProfilerValues[name] = std::vector<float>(m_SampleCount, 0.0f);
		m_ProfilerAverage[name] = 0.0f;
	}

	m_ProfilerColors[name] = color;
	
	m_ProfilerAverage[name] += value;
	m_ProfilerAverage[name] -= m_ProfilerValues[name][m_End];

	m_ProfilerValues[name][m_End] = value;

	if (name == "Render terrain")
	{
		static std::ofstream outFile("out.txt");
		outFile << value << ",";
	}
}

void ProfilerGUI::nextFrame()
{
	m_End++;
	if (m_End >= m_SampleCount || m_End <= m_Start)
		m_Start = (m_Start + 1) % m_SampleCount;
	m_End = m_End % m_SampleCount;

	if (m_CurrentSamples < m_SampleCount)
		m_CurrentSamples++;
}

void ProfilerGUI::Render(ImVec2 size)
{
	ImGui::BeginChild(m_Name.c_str(), size);

	// Graph Render
	{
		ImGui::BeginChild((m_Name + " Graph").c_str(), { size.x - 350.0f, size.y});

		auto position = ImGui::GetCursorScreenPos();
		auto size = ImGui::GetWindowSize();
		ImDrawList* drawlist = ImGui::GetWindowDrawList();

		float xOffset = size.x / float(m_SampleCount);
		float yMultiplier = size.y / m_MaxValue;

		m_MaxValue = 0.0f;
		for (auto& [name, values] : m_ProfilerValues)
		{
			float prevValue = values[m_Start] * yMultiplier;
			ImVec2 initialPositionStart = { position.x, position.y + size.y };
			ImVec2 initialPositionEnd = { position.x, position.y + size.y - prevValue };

			uint32_t currentIndex = (m_Start + 1) % m_SampleCount;
			while (currentIndex != m_End)
			{
				m_MaxValue = std::max(m_MaxValue, values[currentIndex]);

				float currentValue = values[currentIndex] * yMultiplier;
				initialPositionStart = initialPositionEnd;

				initialPositionEnd.x += xOffset;
				initialPositionEnd.y = position.y + size.y - currentValue;

				drawlist->AddLine(initialPositionStart, initialPositionEnd, m_ProfilerColors[name], m_LineWidth);

				prevValue = currentValue;

				currentIndex = (currentIndex + 1) % m_SampleCount;
			}
		}
		ImGui::EndChild();
	}

	ImGui::SameLine();

	{
		ImGui::BeginChild((m_Name + " Stats").c_str());

		ImDrawList* drawlist = ImGui::GetWindowDrawList();

		for (auto& [name, average] : m_ProfilerAverage)
		{
			ImVec2 position = ImGui::GetCursorScreenPos();
			
			drawlist->AddRectFilled(ImVec2(position.x, position.y + 10.0f), 
				ImVec2(position.x + 15.0f, position.y + 15.0f + 10.0f), m_ProfilerColors[name]);

			ImGui::SetCursorScreenPos(ImVec2(position.x + 20.0f, position.y + 10.0f));
			float avg = (average / (float)m_CurrentSamples);

			ImGui::Text((name + " " + std::format("{:.4f}", avg)).c_str());
		}
		ImGui::EndChild();
	}

	ImGui::EndChild();
}

ProfilerManager::ProfilerManager(ImVec2 position, float width) : Position(position), Width(width)
{ }

void ProfilerManager::addProfiler(const std::string& name, uint32_t sampleCount)
{
	if (m_Profilers.find(name) != m_Profilers.end())
		return;
	m_Profilers[name] = std::make_shared<ProfilerGUI>(name, sampleCount);
}

std::shared_ptr<ProfilerGUI> ProfilerManager::operator[](const std::string& name)
{
	if (m_Profilers.find(name) == m_Profilers.end())
		assert(false);
	return m_Profilers[name];
}

void ProfilerManager::Render()
{
	ImGui::SetNextWindowPos(Position);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Width, Height * m_Profilers.size() + 50), ImVec2(Width, Height * m_Profilers.size() + 50));
	ImGui::Begin(("Renderer Metrics: " + VulkanDevice::getVulkanContext()->GPUName).c_str());

	for (auto& [name, profiler] : m_Profilers)
	{
		profiler->nextFrame();
		profiler->Render({ Width, Height });
	}
	ImGui::End();
}
