workspace "BetterVideoExport"
    configurations { "Release", "Debug" }
    platforms { "x64" }
    location "build"

project "BetterVideoExport"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    architecture "x86_64"
    targetextension ".asi"   -- ponytail: GTA5 loads DLLs renamed .asi

    files { "src/**.cpp", "src/**.c", "src/**.h" }
    includedirs { "src", "src/MinHook", "src/spdlog" }
    buildoptions { "/utf-8" }   -- spdlog/fmt requires it

    filter "configurations:Debug"
        symbols "On"
    filter "configurations:Release"
        optimize "On"
