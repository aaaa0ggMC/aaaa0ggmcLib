cmake_minimum_required(VERSION 3.10)
project(aaaa0ggmcLib)

# 是的，我需要最新的C++版本，虽然我代码可能没用上
# 但是我喜欢凑热闹
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 其实aaaa0ggmcLib没有除了libc/libc++以外的依赖来着
set(CMAKE_EXE_LINKER_FLAGS 
    "-Wl,--allow-multiple-definition -Wl,--rpath,./"
)
# 由于我会使用cmake install然后测试lib，因此也就不需要指定build目录了
include_directories(./include)

aux_source_directory("./src" SRC)
aux_source_directory("./src/data" DATA)

# 动态版本
add_library(aaaa0ggmcLib SHARED ${SRC} ${DATA})
set_target_properties(aaaa0ggmcLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_options(aaaa0ggmcLib PRIVATE -fPIC)
target_link_libraries(aaaa0ggmcLib PUBLIC stdc++exp)
## 这里兼容一下Windows上的处理
target_compile_definitions(aaaa0ggmcLib PUBLIC BUILD_DLL)

# 静态版本
add_library(aaaa0ggmcLib_static STATIC ${SRC} ${DATA})
set_target_properties(aaaa0ggmcLib_static PROPERTIES LINKER_LANGUAGE CXX)
target_link_options(aaaa0ggmcLib_static PRIVATE -fPIC)
target_link_libraries(aaaa0ggmcLib_static PUBLIC stdc++exp)
## 依旧兼容Windows
target_compile_definitions(aaaa0ggmcLib_static PUBLIC BUILD_DLL)

# 输出CLANG
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
install(TARGETS aaaa0ggmcLib aaaa0ggmcLib_static
    EXPORT aaaa0ggmcLibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ./include/alib5
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
install(EXPORT aaaa0ggmcLibTargets
    FILE aaaa0ggmcLibTargets.cmake
    NAMESPACE alib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aaaa0ggmcLib
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfigVersion.cmake"
    VERSION 0.1.0         
    COMPATIBILITY SameMajorVersion
)

configure_file(cmake/aaaa0ggmcLibConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aaaa0ggmcLib
)