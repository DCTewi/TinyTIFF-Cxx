
add_executable(tinytiff_cxx_test)

target_compile_features(tinytiff_cxx_test PRIVATE cxx_std_17)
target_include_directories(tinytiff_cxx_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tinytiff_cxx_test PRIVATE tinytiff_cxx)
target_sources(tinytiff_cxx_test PRIVATE "tiff_cxx_test.cpp")
