# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Should not be needed unless we need to override the default sources:
#set(MAIN_SRCS main/mlab100.c)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mlab100)
