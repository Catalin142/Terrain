 project "Terrain"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	targetdir ("%{wks.location}/bin/" .. outputdir.. "/%{prj.name}")
	objdir    ("%{wks.location}/bin-int/" .. outputdir.. "/%{prj.name}")
	
	includedirs
	{
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.imgui}",
		"%{wks.location}/Terrain/External",
		"%{wks.location}/Terrain/Source",
		"%{wks.location}/Terrain/External/imgui",
		"External/glm",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.zstd}",

	}
	
	defines
	{
		"GLEW_STATIC",
	}

	libdirs 
	{
		"%{wks.location}/Terrain/External/GLFW/lib-vc2022/",
		"%{wks.location}/Terrain/External/vulkan_lib/",
		"%{wks.location}/Terrain/External/zstd/static/",
	}

	links
	{
		"vulkan-1.lib",
		"GLFW",
		"imgui",
		"libzstd_static.lib"
	}

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"External/glm/glm/**.hpp",
		"External/glm/glm/**.inl",
		"External/stb_image/**.h",
		"External/stb_image/**.cpp",
		"External/shaderc/glslc/file_includer.cc",
	}

	filter "system:windows"
		systemversion "latest"
		staticruntime "off"

	filter "configurations:Debug"
		defines "DEBUG=1"
		runtime "Debug"
		symbols "on"
		links
		{
			"shaderc_sharedd.lib",
			"shaderc_utild.lib",
			"spirv-cross-cored.lib",
			"spirv-cross-glsld.lib",
			"SPIRV-Toolsd.lib",
		}

	filter "configurations:Release"
		defines "RELEASE=1"
		runtime "Release"
		optimize "on"
		links
		{
			"shaderc_shared.lib",
			"shaderc_util.lib",
			"spirv-cross-core.lib",
			"spirv-cross-glsl.lib",
			"SPIRV-Toolsd.lib",
		}

