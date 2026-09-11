cmake_minimum_required(VERSION 3.21)

project(GlobalRules VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
	commonlibsse-ng
	GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
	GIT_TAG v7.5.1
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(commonlibsse-ng)

include("${commonlibsse-ng_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-tinyexpr CONFIG REQUIRED)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.h"
	@ONLY
)

add_commonlibsse_plugin(
	GlobalRules
	NAME "GlobalRules"
	AUTHOR "vuszi"
	VERSION ${PROJECT_VERSION}
	SOURCES
		src/plugin.cpp
		src/Config.cpp
		src/FormId.cpp
		src/Rules.cpp
		src/Condition.cpp
		src/Expression.cpp
		src/Events.cpp
		src/Engine.cpp
		src/Persistence.cpp
)

target_compile_features(GlobalRules PRIVATE cxx_std_23)
target_precompile_headers(GlobalRules PRIVATE PCH.h)
target_include_directories(GlobalRules PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(
	GlobalRules
	PRIVATE
		nlohmann_json::nlohmann_json
		unofficial::tinyexpr::tinyexpr
)

set_target_properties(
	GlobalRules
	PROPERTIES
	PREFIX ""
	SUFFIX ".dll"
)
