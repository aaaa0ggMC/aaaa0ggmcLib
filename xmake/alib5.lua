function generate_aaaa0ggmcLib5(name, type) 
    target(name, function()
        set_kind(type)

        add_defines(
            "GLM_ENABLE_EXPERIMENTAL",
            "ALIB5_ENABLE_REFLECTION"
        )

        if type == "shared" then
            add_defines("BUILD_DLL", {public = false})
            add_rules("utils.symbols.export_all")
        end

        set_languages("c++26")
        add_cxxflags("-freflection", {force = true})

        add_files("modules/alib5/**.cpp")
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