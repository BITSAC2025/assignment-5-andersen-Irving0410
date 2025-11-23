add_library(a5lib A5Lib.cpp)

add_executable(andersen Andersen.cpp)
if (DEFINED SVF_INSTALL_INCLUDE_DIR)
        # Ensure SVF headers (svf/include and svf-llvm/include) are visible to this target
        target_include_directories(a5lib PRIVATE ${SVF_INSTALL_INCLUDE_DIR})
        target_include_directories(andersen PRIVATE ${SVF_INSTALL_INCLUDE_DIR})
else()
        message(WARNING "SVF_INSTALL_INCLUDE_DIR not defined; include paths may be missing")
endif()
target_link_libraries(andersen PRIVATE
        ${SVF_LIB}
        ${LLVM_LIB}
        a5lib
        )
set_target_properties(andersen PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})