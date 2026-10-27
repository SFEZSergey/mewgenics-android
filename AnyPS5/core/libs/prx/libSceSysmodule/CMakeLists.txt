add_library(libSceSysmodule SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MinimalSysmodule.cpp
)

target_include_directories(libSceSysmodule PRIVATE
        ${LIBS_INCLUDE_DIR}
)
if(APS5_NATIVE_GROUP)
    target_link_libraries(libSceSysmodule PRIVATE aps5coreshim)
else()
    target_link_libraries(libSceSysmodule PRIVATE libc libkernel)
endif()
configure_windows_unwind(libSceSysmodule)
