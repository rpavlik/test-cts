# Copyright (c) 2019-2022, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

run_xr_xml_generate(conformance_generator.py function_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/template_function_info.cpp)

file(GLOB LOCAL_HEADERS "*.h")
file(GLOB LOCAL_SOURCE "*.cpp")
file(GLOB FRAMEWORK_HEADERS "../framework/*.h")
file(GLOB FRAMEWORK_SOURCE "../framework/*.cpp")
file(GLOB ASSETS "composition_examples/*.png"
                 "SourceCodePro-Regular.otf")
file(GLOB VULKAN_SHADERS "vulkan_shaders/*.glsl")

# For including compiled shaders
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(conformance_test SHARED
    ${LOCAL_HEADERS}
    ${LOCAL_SOURCE}
    ${FRAMEWORK_HEADERS}
    ${FRAMEWORK_SOURCE}
    ${CMAKE_CURRENT_BINARY_DIR}/function_info.cpp
    ${VULKAN_SHADERS}
    ${PROJECT_SOURCE_DIR}/src/common/platform_utils.hpp
)
if(ANDROID)
    target_sources(conformance_test PRIVATE $<TARGET_OBJECTS:android_native_app_glue>)
endif()

compile_glsl(run_conformance_test_glsl_compiles ${VULKAN_SHADERS})

add_dependencies(conformance_test
    generate_openxr_header
    run_conformance_test_glsl_compiles
    XrApiLayer_runtime_conformance
)
openxr_add_filesystem_utils(conformance_test)

target_include_directories(conformance_test
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../framework
    ${CMAKE_CURRENT_LIST_DIR}/../../common
    ${PROJECT_SOURCE_DIR}/src/external

    # for openxr.h:
    ${PROJECT_BINARY_DIR}/include

    # for common_config.h:
    ${PROJECT_BINARY_DIR}/src

    # for helper headers
    ${PROJECT_SOURCE_DIR}/external/include
)

if(Vulkan_FOUND)
    target_include_directories(conformance_test
        PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

source_group("Header Files" FILES ${LOCAL_HEADERS})
source_group("Source Files" FILES ${LOCAL_SOURCE})
source_group("Framework Header Files" FILES ${FRAMEWORK_HEADERS})
source_group("Framework Source Files" FILES ${FRAMEWORK_SOURCE})
source_group("Vulkan Shaders" FILES ${VULKAN_SHADERS})

if(Vulkan_LIBRARY)
    target_link_libraries(conformance_test PRIVATE ${Vulkan_LIBRARY})
endif()


if(TARGET openxr-gfxwrapper)
    target_link_libraries(conformance_test PRIVATE openxr-gfxwrapper)
endif()

if(GLSLANG_VALIDATOR AND NOT GLSLC_COMMAND)
    target_compile_definitions(conformance_test PRIVATE USE_GLSLANGVALIDATOR)
endif()

target_link_libraries(conformance_test PRIVATE openxr_loader Threads::Threads)

if(WIN32)
    target_compile_definitions(conformance_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(conformance_test PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX /wd4996)
    endif()
    if(MSVC)
        # Right now can't build this on MinGW because of directxcolors, etc.
        target_link_libraries(conformance_test PRIVATE d3d11 d3d12 d3dcompiler dxgi)
    else()
        target_compile_definitions(conformance_test PRIVATE MISSING_DIRECTX_COLORS)
    endif()

endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(conformance_test PRIVATE -Wall)
    target_link_libraries(conformance_test PRIVATE m)
endif()
if(ANDROID)
    target_sources(
        conformance_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../platform_specific/android_main.cpp"
    )
    target_include_directories(
        conformance_test
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(
        conformance_test
        PRIVATE
        ${ANDROID_LIBRARY}
        ${ANDROID_LOG_LIBRARY}
    )
endif()


if(BUILD_CONFORMANCE_CLI)
    # Copy conformance_test assets files to the conformance_cli binary folder
    foreach(ASSET ${ASSETS})
        add_custom_command(TARGET conformance_test PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${ASSET} $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
        )
        install(FILES ${ASSET}
            DESTINATION conformance)
    endforeach()

    # Copy conformance test binary to conformance_cli binary folder
    add_custom_command(TARGET conformance_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:conformance_test> $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )
endif()


install(
    TARGETS conformance_test
    LIBRARY DESTINATION conformance
    ARCHIVE DESTINATION conformance
    RUNTIME DESTINATION conformance
)
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:conformance_test> DESTINATION conformance OPTIONAL)
endif()
