#### in_vgm (libvgm-based) ####
cmake_minimum_required(VERSION 3.1)
if(POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif()
if(POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif()
project(in_vgm VERSION 0.5 LANGUAGES C CXX)

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(Iconv QUIET)
find_package(libvgm REQUIRED)

if(MSVC)
	set(CMAKE_DEBUG_POSTFIX "d")
	if(NOT MSVC_VERSION LESS 1400)
		add_definitions("/D _CRT_SECURE_NO_WARNINGS")
	endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

# silence typical sound core warnings
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unknown-pragmas")

# additional warnings from http://blog.httrack.com/blog/2014/03/09/what-are-your-gcc-flags/
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -Winit-self -Wstrict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Wformat-nonliteral")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

endif(CMAKE_COMPILER_IS_GNUCC)


set(HEADERS
	FileInfoStorage.hpp
	in_vgm.h
	ini_func.h
	playcfg.hpp
	resource.h
	TagFormatter.hpp
	utils.hpp
)
set(SOURCES
	dlg_cfg.cpp
	dlg_fileinfo.cpp
	extFileInfo.cpp
	FileInfoStorage.cpp
	in_vgm.cpp
	ini_func.c
	playcfg.cpp
	TagFormatter.cpp
	utils.cpp
)
set(RESORUCES
	in_vgm.rc
	icon.ico
	images/logo.bmp
	images/tabicons.bmp
)

add_library(${PROJECT_NAME} SHARED ${HEADERS} ${SOURCES} ${RESORUCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE libvgm::vgm-utils libvgm::vgm-emu libvgm::vgm-player comctl32)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin")
