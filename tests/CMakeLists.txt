Include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF)
set(D_GLIBCXX_USE_CXX11_ABI,"0")
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)

# Testing library
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.9
    #OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(catch2)
# Adds Catch2::Catch2

find_package(Boost)

# Tests need to be added as executables first
add_executable(el3dec_libtest el3dec_libtest.cpp)
target_compile_features(el3dec_libtest PRIVATE cxx_std_17)

#uncomment the next line to add performance benchmarking to the test
target_compile_definitions(el3dec_libtest PUBLIC CATCH_CONFIG_ENABLE_BENCHMARKING)

# Should be linked to the main library, as well as the Catch2 testing library
target_link_libraries(el3dec_libtest PRIVATE el3dec_lib Catch2::Catch2 ${Boost_LIBRARIES})

# If you register a test, then ctest and make test will run it.
# You can also run examples and check the output, as well.
add_test(NAME testel3dec COMMAND el3dec_libtest) # Command can be a target
