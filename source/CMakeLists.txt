add_executable(cpp_tools_test)
target_compile_features(cpp_tools_test PUBLIC cxx_std_23)
target_compile_options(cpp_tools_test PUBLIC "MP" "/Zc:preprocessor")


file(GLOB_RECURSE cpp_tools_cpp_files
    *.cpp, *.cxx, *.c, *.ixx
)
file(GLOB_RECURSE cpp_tools_hxx_files
    *.h
    *.hpp
)

#set_source_files_properties()

target_sources(cpp_tools_test PUBLIC ${cpp_tools_cpp_files} ${cpp_tools_hxx_files})

