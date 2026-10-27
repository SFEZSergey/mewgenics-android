add_library(libSceVideoOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MinimalVideoOut.cpp
)

target_include_directories(libSceVideoOut PRIVATE
        ${LIBS_INCLUDE_DIR}
)
if(APS5_NATIVE_GROUP)
    target_link_libraries(libSceVideoOut PRIVATE aps5coreshim)
else()
    target_link_libraries(libSceVideoOut PRIVATE libc libkernel)
endif()
configure_windows_unwind(libSceVideoOut)
