cmake_minimum_required(VERSION 2.8.3)
project(colormotor_sandbox)

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/)

find_package(Armadillo REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

#find_library(GLFW3_LIB glfw3 ${PROJECT_SOURCE_DIR}/../../../addons/cmx_imgui_app/src/glfw3/lib/linux)

pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
pkg_search_module(PYTHON REQUIRED python>=2.6)

# add_definitions(-Ddefine) # example def

add_definitions(-g)
# make it c++11 compatible
add_definitions(-std=c++11)

include_directories(include
	  ${PROJECT_SOURCE_DIR}/../colormotor/src/
	  ${PROJECT_SOURCE_DIR}/../colormotor
	  ${PROJECT_SOURCE_DIR}/../colormotor/libs/
	  ${PROJECT_SOURCE_DIR}/../colormotor/addons/pycolormotor/
	  ${PROJECT_SOURCE_DIR}/../colormotor/addons/cmx_imgui_app/src
	  ${PROJECT_SOURCE_DIR}/../colormotor/addons/cmx_params/src
	  /usr/local/lib/python2.7/dist-packages/numpy/core/include/
	  ${PYTHON_INCLUDE_DIRS}
	${GTK2_INCLUDE_DIRS}
	${ARMADILLO_INCLUDE_DIRS}
)

link_directories(
	${PROJECT_SOURCE_DIR}/../colormotor/addons/pycolormotor/modules

	${PROJECT_SOURCE_DIR}/../../../lib/
	
	${GTK2_LIBRARY_DIRS}
	${PYTHON_LIBRARY_DIRS}
)

add_definitions(
	${GTK2_CFLAGS_OTHER}
)

add_executable(colormotor_sandbox
	${PROJECT_SOURCE_DIR}/main.cpp
	${PROJECT_SOURCE_DIR}/pyrepl.cpp
	${PROJECT_SOURCE_DIR}/app_wrap.cxx
)

target_link_libraries(colormotor_sandbox
rt
m
dl
Xrandr
Xinerama
Xxf86vm
Xcursor
Xext
Xrender
Xfixes
X11
pthread
xcb
Xau
Xdmcp
glfw
pycm
${OPENGL_LIBRARIES}
${GTK2_LIBRARIES}
${ARMADILLO_LIBRARIES}
${OpenCV_LIBRARIES}
${GLEW_LIBRARIES}
${PYTHON_LIBRARIES}
)







