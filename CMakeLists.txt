cmake_minimum_required(VERSION 2.8.3)
project(colormotor_sandbox)

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/)

set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

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

add_executable(colormotor_sandbox
	${PROJECT_SOURCE_DIR}/main.cpp
	${PROJECT_SOURCE_DIR}/pyrepl.cpp
	${PROJECT_SOURCE_DIR}/app_wrap.cxx
)


if(APPLE)
  set(PYTHON_BASE_DIR $ENV{HOME}/anaconda3/envs/py27)
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

  set(linkdir ${PYTHON_BASE_DIR}/lib)
  link_directories(
    ${PYTHON_BASE_DIR}/lib
  )

  #${ARMADILLO_INCLUDE_DIRS}
  include_directories(
    	  ${PYTHON_BASE_DIR}/lib/python2.7/site-packages/numpy/core/include/
        ${PYTHON_BASE_DIR}/include
        ${PYTHON_BASE_DIR}/include/python2.7
        )

  get_cmake_property(output link_directories)
  message(STATUS ${linkdir}) 

  #${PYTHON_BASE_DIR}/lib/libopencv_core.dylib
  #${PYTHON_BASE_DIR}/lib/libopencv_imgproc.dylib
  #${PYTHON_BASE_DIR}/lib/libopencv_highgui.dylib
  #${PYTHON_BASE_DIR}/lib/libopencv_imgcodecs.dylib
  #opencv_core
  #opencv_imgproc
  #opencv_highgui
  #opencv_imgcodecs
  
  target_link_libraries(colormotor_sandbox
    ${PYTHON_BASE_DIR}/lib/libopencv_core.dylib
		${PYTHON_BASE_DIR}/lib/libopencv_imgproc.dylib
		${PYTHON_BASE_DIR}/lib/libopencv_highgui.dylib
		${PYTHON_BASE_DIR}/lib/libopencv_imgcodecs.dylib
		${PYTHON_BASE_DIR}/lib/libarmadillo.dylib
		lapack
		blas
		${PYTHON_BASE_DIR}/lib/libglfw.dylib
    pycm
		${PYTHON_BASE_DIR}/lib/libpython2.7.dylib
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











