include(${CMAKE_SOURCE_DIR}/cmake/submodules.cmake)
initialize_submodule(imgui)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(OpenGL REQUIRED)
set(IMGUI_SOURCES
		${CMAKE_SOURCE_DIR}/deps/imgui/imgui.cpp
		${CMAKE_SOURCE_DIR}/deps/imgui/imgui_draw.cpp
		${CMAKE_SOURCE_DIR}/deps/imgui/imgui_tables.cpp
		${CMAKE_SOURCE_DIR}/deps/imgui/imgui_widgets.cpp
		${CMAKE_SOURCE_DIR}/deps/imgui/backends/imgui_impl_sdl2.cpp
		${CMAKE_SOURCE_DIR}/deps/imgui/backends/imgui_impl_opengl3.cpp
)
add_executable(ethdebug-tool
		src/main.cpp
		src/ui/UserInterface.cpp
		src/ui/UserInterface.h
		src/ui/Styles.h
		src/ui/HighlightedTextView.cpp
		src/ui/HighlightedTextView.h
		src/ui/InstructionTableView.h
		src/ui/InstructionTableView.cpp
		src/context/CodeContextDecoder.h
		src/Compiler.cpp
		src/Compiler.h
		src/data/InstructionInfo.h
		${IMGUI_SOURCES}
)
target_include_directories(ethdebug-tool PRIVATE
		${CMAKE_SOURCE_DIR}/tools/ethdebug/src
		${CMAKE_SOURCE_DIR}/deps
		${CMAKE_SOURCE_DIR}/deps/imgui
		${SDL2_INCLUDE_DIRS}
)
target_link_libraries(ethdebug-tool PRIVATE -L${SDL2_LIBRARY_DIRS} ${SDL2_LIBRARIES} OpenGL::GL solidity Boost::boost Boost::program_options)
