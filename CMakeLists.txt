cmake_minimum_required(VERSION 3.26)

# Set project
set (QuikQtPluginLib "QuikQtBridge")
string(TOUPPER ${QuikQtPluginLib} UCASELIBNAME)

project(${QuikQtPluginLib} LANGUAGES CXX)

# Set Qt properties
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Set C++ properties
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "/MD")
set(CMAKE_CXX_FLAGS_DEBUG "/MD")

# Set CMAKE properties
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Find packages
find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_standard_project_setup()

configure_file(
  quikcoastbootstrap.h.in
  generated/quikcoastbootstrap.h
  @ONLY
)

configure_file(
  quikcoastbootstrap.cpp.in
  generated/quikcoastbootstrap.cpp
  @ONLY
)

# Set library files
set(lib_files
  qtmain.cpp

  quikcoast.h
  quikcoast.cpp

  quikqtbridge.h
  quikqtbridge.cpp

  serverconfigreader.h
  serverconfigreader.cpp

  bridgetcpserver.cpp
  jsonprotocolhandler.cpp
)

qt_wrap_cpp(moc_files
  bridgetcpserver.h
  jsonprotocolhandler.h
)

add_library(${QuikQtPluginLib} SHARED
  QuikQtBridge_global.h
  ${lib_files} ${moc_files}
  ${CMAKE_CURRENT_BINARY_DIR}/generated/quikcoastbootstrap.cpp
)

target_link_libraries(${QuikQtPluginLib} PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(${QuikQtPluginLib} PRIVATE Qt${QT_VERSION_MAJOR}::Network)

target_compile_definitions(${QuikQtPluginLib} PRIVATE ${UCASELIBNAME}_LIBRARY)
target_compile_definitions(${QuikQtPluginLib} PRIVATE QUIK_QT_PLUGIN_LIB_NAME=${QuikQtPluginLib})

target_include_directories(${QuikQtPluginLib} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(${QuikQtPluginLib} PRIVATE "C:/Dev/Quik/Lua-5.4.2/include")
target_link_directories(${QuikQtPluginLib} PRIVATE "C:/Dev/Quik/Lua-5.4.2/fromquik")
target_link_libraries(${QuikQtPluginLib} PRIVATE lua54)
