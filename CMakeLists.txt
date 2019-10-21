cmake_minimum_required (VERSION 3.4.3)

set (CMAKE_CXX_STANDARD 11)

set (CMAKE_BUILD_TYPE Debug)

#set (CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

project (disassembler)

find_file ( COMGR_INC NAMES "amd_comgr.h" PATHS ${COMGR_INC_PATH} )
find_library ( COMGR_LIB "libamd_comgr.so" ${COMGR_LIB_PATH} )
get_filename_component ( COMGR_INC_PATH ${COMGR_INC} DIRECTORY CACHE )
get_filename_component ( COMGR_LIB_PATH ${COMGR_LIB} DIRECTORY CACHE )
unset( COMGR_INC CACHE )
unset( COMGR_LIB CACHE )

include_directories ( ${COMGR_INC_PATH} )
link_directories ( ${COMGR_LIB_PATH} )

add_executable (disassembler disassembler.cpp)

target_link_libraries (disassembler amd_comgr)
