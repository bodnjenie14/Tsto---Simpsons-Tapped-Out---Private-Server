dependencies = {
	basePath = "./deps"
}

function dependencies.load()
	dir = path.join(dependencies.basePath, "premake/*.lua")
	deps = os.matchfiles(dir)
	for i, dep in pairs(deps) do
		dep = dep:gsub(".lua", "")
		require(dep)
	end
end

function dependencies.imports()
	for i, proj in pairs(dependencies) do
		if type(i) == 'number' then
			proj.import()
		end
	end
end

function dependencies.projects()
    for i, proj in pairs(dependencies) do
        if type(i) == 'number' and proj.project then  -- Check if 'project' function exists
            proj.project()  -- Only call if the project function exists
        end
    end
end


newoption {
	trigger = "copy-to",
	description = "Optional, copy the EXE to a custom folder after build, define the path here if wanted.",
	value = "PATH"
}

newoption {
	trigger = "dev-build",
	description = "Enable development builds of the client."
}

dependencies.load()

-- Define the main project 

workspace "simpsons_server_bodnjenie"

	startproject "server"
	location "./build"
	objdir "%{wks.location}/obj"
	targetdir "%{wks.location}/bin/%{cfg.platform}/%{cfg.buildcfg}"

	configurations {"Debug", "Release"}

	language "C++"
	cppdialect "C++20"

	architecture "x86_64"
	platforms "x64"
	staticruntime "off"
    runtime "Release"

	systemversion "latest"
	symbols "On"
	editandcontinue "Off"
	warnings "Extra"
	characterset "ASCII"

	if _OPTIONS["dev-build"] then
		defines {"DEV_BUILD"}
	end

	if os.getenv("CI") then
		defines {"CI"}
	end

    flags { "NoIncrementalLink", "NoMinimalRebuild", "MultiProcessorCompile", "No64BitChecks" }

	filter "platforms:x64"
		defines {"_WINDOWS", "WIN32"}
	filter {}

	filter "configurations:Release"
		optimize "Size"
		buildoptions {"/GL", "/Zc:__cplusplus"}
		linkoptions { "/IGNORE:4702", "/LTCG" }
		defines {"NDEBUG"}
	filter {}

	filter "configurations:Debug"
		--optimize "Debug"
		--defines {"DEBUG", "_DEBUG"}
                runtime "Debug"
		defines { "_DEBUG" }
                buildoptions { "/Zc:__cplusplus" }
   	        symbols "On"
	filter {}

-- Define the evpp project (now in source/evpp)
project "evpp"
    kind "StaticLib"
    language "C++"
    --cppdialect "C++17"
    staticruntime "off"

    includedirs {
        "source/evpp",
        "source/evpp/thirdparty/libevent/",
        "source/evpp/thirdparty/glog/",
        "source/evpp/thirdparty/gflags/"
    }

    files {
        "source/evpp/evpp/**.h",
        "source/evpp/evpp/**.cc",
        "source/evpp/thirdparty/libevent/include/**.cc",
        "source/evpp/thirdparty/glog/include/**.cc",
        "source/evpp/thirdparty/gflags/include/**.cc"
    }

    filter "system:windows"
        links { "Ws2_32" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"


		
-- Define the utlis project
project "utilities"
	kind "StaticLib"
	language "C++"

	files {"./source/utilities/**.hpp", "./source/utilities/**.cpp"}

	includedirs {"./source/utilities", "%{prj.location}/source"}

	dependencies.imports()

	
-- Define the server project
project "server"
    kind "ConsoleApp"
    language "C++"
    targetname "tsto_server"

    pchheader "std_include.hpp"
    pchsource "source/server/std_include.cpp"

    files {
        "./source/server/**.rc", 
        "./source/server/**.hpp", 
        "./source/server/**.cpp", 
	"./source/server/**.proto",
        "./source/server/resources/**.*",
	"build/src/**.h",
	"build/src/**.cc"
    }

    includedirs {
        "./source/server", 
        "./source/utilities", 
        "./source/evpp",  -- evpp 
        "./deps/google/protobuf/include",
	"./build/src/protobuf/generated/", 
        "%{prj.location}/source"
    }
	
	
    links {
        "utilities",  -- Links with utilities
        "evpp",       -- Links with evpp
        "./source/evpp/3rdparty/libevent/lib/event.lib", -- Links with eventlib [EVENT]
        "./source/evpp/3rdparty/libevent/lib/event_core.lib", -- Links with eventlib [CORE]
        "./source/evpp/3rdparty/libevent/lib/event_extra.lib", --Links with eventlib [EVENT EXTRA]
        "./source/evpp/3rdparty/glog/lib/glog.lib", -- Links with glog 
        "./source/evpp/3rdparty/gflags/lib/gflags.lib", -- Links with gflags

    }

	
    -- Apply precompiled headers to moc files
    filter "files:src/server/protobuf/generated/**.cc"
    pchheader "std_include.hpp"
    pchsource "src/server/std_include.cpp"

    filter {}
	
    protobuf.generate() 


	
    dependencies.imports()

group "Dependencies"
    dependencies.projects()