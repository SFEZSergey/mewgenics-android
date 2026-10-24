# Мост нативной группы AnyPS5 под box64 (см. Aps5Native.hpp).
if(APS5_NATIVE_GROUP)
    # Нативная сторона: функции ядра (libc/libkernel) для драйвера и вывода, переадресуемые в x86.
    add_library(aps5coreshim SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/CoreShim.cpp ${CMAKE_CURRENT_SOURCE_DIR}/WriteWatchNative.cpp)
    target_include_directories(aps5coreshim PRIVATE ${LIBS_INCLUDE_DIR})
    set_target_properties(aps5coreshim PROPERTIES PREFIX "lib" OUTPUT_NAME "aps5coreshim" SUFFIX ".so")
endif()
