# for each subdirectory containing a CMakeLists.txt, add that subdirectory

set(BUILD_SHARED_LIBS OFF)
set(ROCKSDB_STATIC ${HIVE_STATIC_BUILD})

#Configure needed build options of RocksDB.
SET(WITH_TESTS OFF CACHE BOOL "build with tests")
SET(WITH_SNAPPY ON CACHE BOOL "build with SNAPPY")
SET(WITH_ZLIB OFF CACHE BOOL "build with ZLIB")
SET(WITH_BZ2 OFF CACHE BOOL "build with BZ2")
SET(WITH_BENCHMARKS OFF CACHE BOOL "build with BENCHMARKS")
SET(WITH_GFLAGS OFF CACHE BOOL "build with GFlags")
SET(USE_RTTI ON CACHE BOOL "build with RTTI")
SET(PORTABLE ON CACHE BOOL "build a portable binary")
SET(WITH_TESTS OFF CACHE BOOL "build with tests")
SET(WITH_TOOLS OFF CACHE BOOL "build with tools")

# If we don't have CMake variables defined, try to get them from the environment
if (NOT DEFINED ZLIB_ROOT_DIR)
   if (EXISTS "$ENV{ZLIB_ROOT_DIR}")
      set(ZLIB_ROOT_DIR $ENV{ZLIB_ROOT_DIR})
   endif()
endif()
if (NOT DEFINED BZIP2_ROOT_DIR)
   if (EXISTS "$ENV{BZIP2_ROOT_DIR}")
      set(BZIP2_ROOT_DIR $ENV{BZIP2_ROOT_DIR})
   endif()
endif()

file( GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} * )
foreach( child ${children} )
   if( IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" )
      if( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt" )
         add_subdirectory( "${child}" )
      endif()
   endif()
endforeach()

