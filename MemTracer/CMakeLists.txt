if(NOT QBDI_ARCH_X86_64)
    message(FATAL_ERROR "MemTracer: Unsupported architecture: only X86_64 available")
endif()

add_library(tracer SHARED tracer.cpp preload.cpp)
target_link_libraries(tracer QBDIPreload QBDI_static)
set_target_properties(tracer 
                      PROPERTIES CXX_STANDARD 14
                      CXX_STANDARD_REQUIRED ON)
add_executable(example example/bintree.c)
