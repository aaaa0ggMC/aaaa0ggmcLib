includes("xmake/package.lua")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

if is_plat("mingw", "msys") then
    add_requires("pacman::glm", {alias = "glm"})
    add_requires("pacman::rapidjson", {alias = "rapidjson"})
    add_requires("pacman::tomlplusplus", {alias = "toml++"})
    add_requires("pacman::gtest", {alias = "gtest"})
else
    add_requires("glm", "rapidjson" , "toml++", "gtest")
end

add_rpathdirs(".")
add_cxflags("-fno-omit-frame-pointer")
if is_mode("release") then
    set_optimize("fastest")
    add_cxflags("-O3", "-DNDEBUG")
elseif is_mode("debug") then
    set_optimize("none")
    set_symbols("debug")
    add_cxflags("-g", "-O0")
end
add_ldflags("-fPIC") 
add_ldflags("-Wl,--allow-multiple-definition")

--- generators
includes("xmake/alib5.lua")
includes("xmake/alib6.lua")

generate_aaaa0ggmcLib5("aaaa0ggmcLib", "shared")
generate_aaaa0ggmcLib5("aaaa0ggmcLib-static", "static")

generate_aaaa0ggmcLib6("aaaa0ggmcLib6", "shared")
generate_aaaa0ggmcLib6("aaaa0ggmcLib6-static", "static")

--- tests

target("test6", function()
    set_kind("binary")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true})
    add_syslinks("stdc++exp")

    add_files("modules/alib6_test/**.cpp")
    add_deps("aaaa0ggmcLib6")
end)

target("gtest6", function()
    set_kind("binary")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true})
    add_syslinks("stdc++exp")

    add_packages("gtest")
    add_files("tests/alib6/**.cpp")
    add_deps("aaaa0ggmcLib6")
end)

target("bench6", function()
    set_kind("binary")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true})
    add_syslinks("stdc++exp")

    add_files("benchmarks/alib6/**.cpp")
    add_deps("aaaa0ggmcLib6")
end)

target("bench_log", function()
    set_kind("binary")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true})
    add_syslinks("stdc++exp")

    add_files("benchmarks/log_compare/**.cpp")
    add_includedirs("include", {public = true})
    add_deps("aaaa0ggmcLib", "aaaa0ggmcLib6")
end)