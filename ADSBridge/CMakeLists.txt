# CMakeList.txt : CMake project for ADSBridge, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)



# Add source to this project's executable.
add_executable (ADSBridge "ADSBridge.cpp" "ADSBridge.h")
target_link_libraries (ADSBridge "C:/TwinCAT/AdsApi/TcAdsDll/x64/lib/TcAdsDll.lib")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ADSBridge PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
