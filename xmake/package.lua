package("aaaa0ggmclib")
    set_description("aaaa0ggmc's toolkit library.")
    set_urls("https://github.com/aaaa0ggMC/aaaa0ggmcLib.git")
    
    add_configs("shared", {description = "Build shared library.", default = false, type = "boolean"})
    
    on_install(function (package)
        local configs = {}
        local target_name = package:config("shared") and "aaaa0ggmcLib" or "aaaa0ggmcLib-static"
        import("package.tools.xmake").install(package, configs)
    end)

    on_fetch(function (package)
        local result = {}
        result.linkdirs = path.join(package:installdir(), "lib")
        result.includedirs = path.join(package:installdir(), "include")
        
        if package:config("shared") then
            result.links = {"aaaa0ggmcLib"}
        else
            result.links = {"aaaa0ggmcLib-static"}
        end
        return result
    end)
package_end()