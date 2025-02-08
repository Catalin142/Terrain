workspace "Terrain"
	architecture "x64"
	configurations { "Debug", "Release" }
	startproject "Terrain"
	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

	IncludeDir = {}
	IncludeDir["GLFW"] = "%{wks.location}/Terrain/External/GLFW/include"
	IncludeDir["glm"] = "%{wks.location}/Terrain/External/glm"
	IncludeDir["stb_image"] = "%{wks.location}/Terrain/External/stb_image"
	IncludeDir["imgui"] = "%{wks.location}/Terrain/External/imgui"
	IncludeDir["zstd"] = "%{wks.location}/Terrain/External/zstd/include"
	
	group "Dependencies"
	include "Terrain/External/GLFW"
	include "Terrain/External/imgui"
	group ""

	include "Terrain"
