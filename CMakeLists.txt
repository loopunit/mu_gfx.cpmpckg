cmake_minimum_required( VERSION 3.12 )

project(mu_gfx
	VERSION 1.7.8)

include(CMakeDependentOption)

option(MU_GFX_BUILD_TESTS "Build tests." OFF)

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(ExternalProject)

####

if (NOT DEFINED cpmpckg_SOURCE_DIR)
	include(cmake/CPM.cmake)

	# One frustrating part of this project setup is the circular dependency between cpmpckg and this repo.
	# GIT_TAG will always lag cpmpckg @ HEAD when this project is updated there.
	CPMAddPackage(
		NAME cpmpckg
		GITHUB_REPOSITORY loopunit/cpmpckg
		GIT_TAG 60b9fa090fd890cc7f96481d7ac81ed449202ce0
		DOWNLOAD_ONLY true)

	include(${cpmpckg_SOURCE_DIR}/cmake/add_cpm_module.cmake)
else()
	set(CPM_SCRIPTS ${cpmpckg_SOURCE_DIR}/cmake)
	include(${cpmpckg_SOURCE_DIR}/cmake/CPM.cmake)
	include(${cpmpckg_SOURCE_DIR}/cmake/add_cpm_module.cmake)
endif()

####

CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY loopunit/PackageProject.cmake
  GIT_TAG e5ec20069766f4f078f9f01a86e250e20da0817c)

####

CPMAddBaseModule(framegraph)
CPMAddBaseModule(basis_universal)
CPMAddBaseModule(glfw)
CPMAddBaseModule(mu_stdlib)

set(mu_gfx_SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})

file(GLOB gfx_impl_headers)
file(GLOB gfx_impl_sources)

file(GLOB gfx_impl_sources2 
	"${mu_gfx_SOURCE_ROOT}/src/glfw_vulkan/VulkanDevice2.h"
	"${mu_gfx_SOURCE_ROOT}/src/glfw_vulkan/VulkanDevice2.cpp"
	"${mu_gfx_SOURCE_ROOT}/src/glfw_vulkan/main.cpp")

list(APPEND gfx_impl_sources ${gfx_impl_sources2})

file(GLOB gfx_headers
	"${mu_gfx_SOURCE_ROOT}/include/*.h")

file(GLOB gfx_sources
	"${mu_gfx_SOURCE_ROOT}/src/mu_gfx.cpp"
	"${mu_gfx_SOURCE_ROOT}/src/mu_gfx_impl.h")

add_library(mu_gfx STATIC ${gfx_sources} ${gfx_impl_sources} ${gfx_headers} ${gfx_impl_headers})

target_include_directories(mu_gfx PUBLIC $<BUILD_INTERFACE:${mu_gfx_SOURCE_ROOT}/include>
					$<INSTALL_INTERFACE:mu_gfx>)

target_link_libraries(mu_gfx
	PUBLIC
		cpm_install::glfw cpm_install::framegraph cpm_install::basis_universal cpm_install::mu_stdlib)

set_target_properties(mu_gfx PROPERTIES CXX_STANDARD 17)

packageProject(
	NAME mu_gfx
	VERSION ${PROJECT_VERSION}
	BINARY_DIR ${PROJECT_BINARY_DIR}
	INCLUDE_DIR ${mu_gfx_SOURCE_ROOT}/include
	INCLUDE_DESTINATION include)

if(MSVC)
  if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set_target_properties(mu_gfx PROPERTIES IMPORTED_CONFIGURATIONS "Debug")
    set_target_properties(mu_gfx PROPERTIES COMPILE_PDB_NAME mu_gfx COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib" )
  elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set_target_properties(mu_gfx PROPERTIES IMPORTED_CONFIGURATIONS "RelWithDebInfo")
    set_target_properties(mu_gfx PROPERTIES COMPILE_PDB_NAME mu_gfx COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib" )
  endif()
endif()

if(MU_GFX_BUILD_TESTS)
	file(GLOB test_sources 
		${CMAKE_CURRENT_LIST_DIR}/tests/main.cpp)

	add_executable(mu_gfx_test
		${test_sources})

	set_target_properties(mu_gfx_test PROPERTIES CXX_STANDARD 17)

	target_include_directories(mu_gfx_test 
		PRIVATE 
			${mu_gfx_SOURCE_DIR}/tests)

	target_link_libraries(mu_gfx_test 
		PUBLIC
			mu_gfx)
endif()
