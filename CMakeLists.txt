cmake_minimum_required(VERSION 2.8.3)
project(colormotor_sandbox)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set( CMAKE_VERBOSE_MAKEFILE on )
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/)

#set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

#find_package(Armadillo REQUIRED)

#find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
#find_package(GLEW REQUIRED)

#find_library(GLFW3_LIB glfw3 ${PROJECT_SOURCE_DIR}/../../../addons/cmx_imgui_app/src/glfw3/lib/linux)



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
)

#${PROJECT_SOURCE_DIR}/../../../lib/
link_directories(
	${PROJECT_SOURCE_DIR}/../colormotor/addons/pycolormotor/modules
)

find_package(PythonLibs REQUIRED)

if(APPLE)
  set(BASE_SEARCH_DIR $ENV{HOME}/opt/miniconda3)
  message("Base dir is " ${BASE_SEARCH_DIR})
  set(ARMADILLO_LIBRARIES
		${ARMADILLO_LIBRARIES}
		lapack
		blas)
	

  # Nifty trick from http://stackoverflow.com/questions/24174394/cmake-is-not-able-to-find-python-libraries

  execute_process(COMMAND python -c "import sys; from distutils.sysconfig import get_python_inc; sys.stdout.write(get_python_inc())" OUTPUT_VARIABLE PYTHON_INCLUDE_DIRS)

  execute_process(COMMAND python -c "import sys; import distutils.sysconfig as sysconfig; sys.stdout.write(sysconfig.get_config_var('LIBDIR'))" OUTPUT_VARIABLE PYTHON_LIBRARY_DIRS)

  execute_process(COMMAND python -c "import sys; import distutils.sysconfig as sysconfig; sys.stdout.write(sysconfig.get_config_var('LDLIBRARY'))" OUTPUT_VARIABLE PYTHON_LIBRARIES)

  # One more hack to setup PYTHONHOME for the interpreter... arghhh
  execute_process(COMMAND python -c "import sys; import distutils.sysconfig as sysconfig; sys.stdout.write(sysconfig.get_config_var('exec_prefix'))" OUTPUT_VARIABLE PYTHON_HOME)
  add_definitions(-DPYTHON_HOME="${PYTHON_HOME}")

  link_directories(
    ${BASE_SEARCH_DIR}/lib
    ${PROJECT_SOURCE_DIR}/../colormotor/addons/cmx_imgui_app/src/glfw3/lib/osx
    ${PYTHON_LIBRARY_DIRS}
  )

  #${ARMADILLO_INCLUDE_DIRS}
  include_directories(
    	  ${BASE_SEARCH_DIR}/lib/python3.8/site-packages/numpy/core/include/
        ${BASE_SEARCH_DIR}/include
        ${PYTHON_INCLUDE_DIRS}
        ${BASE_SEARCH_DIR}/include/opencv4
        ${PROJECT_SOURCE_DIR}/../colormotor/addons/cmx_imgui_app/src/glfw3/include
        )
  
  #set(CMAKE_EXE_LINKER_FLAGS "-rpath ${BASE_SEARCH_DIR}/lib")

    add_executable(colormotor_sandbox
	${PROJECT_SOURCE_DIR}/main.cpp
	${PROJECT_SOURCE_DIR}/pyrepl.cpp
	${PROJECT_SOURCE_DIR}/app_wrap.cxx
  )

  

  target_link_libraries(colormotor_sandbox
    opencv_core
		opencv_imgproc
		opencv_highgui
    opencv_imgcodecs
    opencv_videoio
		armadillo
		LAPACK
		blas
		glfw3
		${PYTHON_LIBRARY_DIRS}/${PYTHON_LIBRARIES}
    pycm
		"-framework CoreFoundation"
		"-framework OpenGL"	
		"-framework Cocoa"
		"-framework AppKit"
		"-framework CoreVideo"
		"-framework QTKit"
		"-framework IOKit"
		"-framework AGL"
		"-framework GLUT"		
	)
    
elseif(UNIX)
  pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
  #pkg_search_module(PYTHON REQUIRED python>=2.6)
  #find_package(PythonLibs)
  find_package(PkgConfig REQUIRED)

  # Nifty trick from http://stackoverflow.com/questions/24174394/cmake-is-not-able-to-find-python-libraries

  execute_process(COMMAND python -c "import sys; from distutils.sysconfig import get_python_inc; sys.stdout.write(get_python_inc())" OUTPUT_VARIABLE PYTHON_INCLUDE_DIRS)

  execute_process(COMMAND python -c "import sys; import distutils.sysconfig as sysconfig; sys.stdout.write(sysconfig.get_config_var('LIBDIR'))" OUTPUT_VARIABLE PYTHON_LIBRARY_DIRS)

  execute_process(COMMAND python -c "import sys; import distutils.sysconfig as sysconfig; sys.stdout.write(sysconfig.get_config_var('LDLIBRARY'))" OUTPUT_VARIABLE PYTHON_LIBRARIES)

  # One more hack to setup PYTHONHOME for the interpreter... arghhh
  execute_process(COMMAND python -c "import sys; import distutils.sysconfig as sysconfig; sys.stdout.write(sysconfig.get_config_var('exec_prefix'))" OUTPUT_VARIABLE PYTHON_HOME)
  add_definitions(-DPYTHON_HOME="${PYTHON_HOME}")

include_directories(
    	  ${PYTHON_LIBRARY_DIRS}/python2.7/dist-packages/numpy/core/include/
	      ${PYTHON_INCLUDE_DIRS}
	      ${GTK2_INCLUDE_DIRS}
	      ${ARMADILLO_INCLUDE_DIRS}
        )

  add_definitions(
	${GTK2_CFLAGS_OTHER}
)

link_directories(
	${GTK2_LIBRARY_DIRS}
	${PYTHON_LIBRARY_DIRS}
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
  ${PYTHON_LIBRARY_DIRS}/libopencv_core.so
  ${PYTHON_LIBRARY_DIRS}/libopencv_imgproc.so
  ${PYTHON_LIBRARY_DIRS}/libopencv_legacy.so
  ${PYTHON_LIBRARY_DIRS}/libopencv_highgui.so
  ${OPENGL_LIBRARIES}
  ${GTK2_LIBRARIES}
  ${ARMADILLO_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${PYTHON_LIBRARY_DIRS}/${PYTHON_LIBRARIES}
  )

endif()












