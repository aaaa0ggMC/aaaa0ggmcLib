includes("xmake/package.lua")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

if is_plat("mingw", "msys") then
    add_requires("pacman::glm", {alias = "glm"})
    add_requires("pacman::rapidjson", {alias = "rapidjson"})
    add_requires("pacman::tomlplusplus", {alias = "toml++"})
else
    add_requires("glm", "rapidjson" , "toml++")
end

set_languages("c++26")
add_rpathdirs(".")

add_cxflags("-fno-omit-frame-pointer")
add_cxxflags("-freflection", {force = true})
add_ldflags("-fPIC") 
add_ldflags("-Wl,--allow-multiple-definition")

add_defines(
    "GLM_ENABLE_EXPERIMENTAL",
    "ALIB5_ENABLE_REFLECTION"
)

function generate_aaaa0ggmcLib(name, type) 
    target(name, function()
        set_kind(type)
        add_files("src/**.cpp")
        add_includedirs("include", {public = true})
        add_headerfiles("include/(alib5/**.h)")
        add_packages("glm", "rapidjson", "toml++")

        -- Network
        add_defines("ASIO_STANDALONE", "BUILD_DLL", { public = false})
        if is_plat("windows", "mingw") then 
            add_defines("_WIN32_WINNT=0x0601" , {public = false})
            add_syslinks("ws2_32", "mswsock", {public = false})
        end 
        
        add_syslinks("stdc++exp", {public = true})
    end)
end

generate_aaaa0ggmcLib("aaaa0ggmcLib", "shared")
generate_aaaa0ggmcLib("aaaa0ggmcLib-static", "static")