cmake_minimum_required(VERSION 3.18.4)

set(QF_BUILD_QML_PLUGINS ON CACHE BOOL "Build with QML Plugins support")

project(quickbox LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

include(CTest)
set(CMAKE_SHARED_LIBRARY_PREFIX "") # we don't want CMake to prepend "lib" to our libraries, we prefer adding that ourselves

add_compile_definitions(COMMIT_SHA="${COMMIT_SHA}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic ${CMAKE_CXX_FLAGS}")
endif()

if (WIN32)
	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		set(CMAKE_WIN32_EXECUTABLE ON) # Don't open a console window for Windows apps on Relase mode.
	endif()
endif (WIN32)

if(QF_USE_LOCAL_NECROLOG)
    find_package(necrolog REQUIRED)
else()
    if (NOT TARGET libnecrolog)
        add_subdirectory(3rdparty/necrolog)
    endif()
endif()

set(USE_QT6 ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Sql Qml Xml LinguistTools PrintSupport Svg SerialPort Multimedia Network)
set(Qt_FOUND ${Qt6_FOUND})

if(Qt_FOUND)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC ON)
	set(CMAKE_AUTORCC ON)

	add_subdirectory(libqf)
	if(QF_LIB_ONLY)
		message(STATUS "Building libqf only")
	elseif(QF_QSQLMON_ONLY)
		message(STATUS "Building libqf and qsqlmon only")
		add_subdirectory(tools/qsqlmon)
	else()
		add_subdirectory(libsiut)
		add_subdirectory(libquickevent)
		add_subdirectory(quickevent)
		add_subdirectory(quickhttpd)
		add_subdirectory(quickshow)
		add_subdirectory(tools/qsqlmon)
	endif()
else()
	message(FATAL_ERROR "Qt not found")
endif()

# Directories where Qt Creator can find QML files.
# (Not needed for builds, but makes Qt Creator code completion happy.)
list(APPEND QML_IMPORT_PATH ${CMAKE_BINARY_DIR}/libqf/plugins)
#list(APPEND QML_IMPORT_PATH "/second/example/path/to/qml")

# Prevent adding duplicate values at each run of CMake.
list(REMOVE_DUPLICATES QML_IMPORT_PATH)

# The variable is cached in ${BUILD_DIR}/CMakeCache.txt. We need FORCE to
# change it there immediately. Also, add a comment to the cache file.
set(QML_IMPORT_PATH ${QML_IMPORT_PATH}
	CACHE STRING "Qt Creator extra qml import paths"
	FORCE
)
