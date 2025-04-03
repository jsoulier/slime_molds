cmake_minimum_required(VERSION 3.24)
project(slime_molds)

set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
make_directory(${BINARY_DIR})

add_subdirectory(SDL)
add_executable(slime_molds WIN32 main.cpp stb.c)
set_target_properties(slime_molds PROPERTIES CXX_STANDARD 23)
target_link_libraries(slime_molds PUBLIC SDL3::SDL3)

function(spirv FILE)
    set(OUTPUT ${BINARY_DIR}/${FILE})
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND glslc ${FILE} -o ${OUTPUT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${FILE}
        BYPRODUCTS ${OUTPUT}
        COMMENT ${FILE}
    )
    string(REPLACE . _ NAME ${FILE})
    add_custom_target(${NAME} DEPENDS ${OUTPUT})
    add_dependencies(slime_molds ${NAME})
endfunction()
spirv(diffuse.frag)
spirv(draw.frag)
spirv(fullscreen.vert)
spirv(update.comp)

configure_file(LICENSE.txt ${BINARY_DIR} COPYONLY)
configure_file(README.md ${BINARY_DIR} COPYONLY)