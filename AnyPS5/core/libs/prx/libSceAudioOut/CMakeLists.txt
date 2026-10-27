add_library(libSceAudioOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MinimalAudioOut.cpp
)

target_include_directories(libSceAudioOut PRIVATE
        ${LIBS_INCLUDE_DIR}
)
if(APS5_NATIVE_GROUP)
    target_link_libraries(libSceAudioOut PRIVATE aps5coreshim)
else()
    target_link_libraries(libSceAudioOut PRIVATE libc libkernel)
endif()
configure_windows_unwind(libSceAudioOut)

