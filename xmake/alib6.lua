function generate_aaaa0ggmcLib6(name, type) 
    target(name, function()
        set_kind(type)
        set_languages("c++26")
        add_cxxflags("-freflection", {force = true, public = true})

        if type == "shared" then
            add_rules("utils.symbols.export_all")
        end

        add_includedirs("include", {public = true})

        add_files("include/alib6/**.cppm", {public = true})
        add_files("modules/alib6/**.cpp")
        
        add_headerfiles("include/(alib6/**.cppm)")
        add_headerfiles("include/(alib6/**.h)")
        add_syslinks("stdc++exp", {public = true})
    end)
end