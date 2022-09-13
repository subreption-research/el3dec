add_executable(el3dec_app app.cpp)
add_executable(el3dec_netdaemon netdaemon.cpp)

target_compile_features(el3dec_app PRIVATE cxx_std_17)
target_compile_features(el3dec_netdaemon PRIVATE cxx_std_17)

# This depends on (header only) boost
set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF)
set(_Boost_LOG_SETUP_DEPENDENCIES log)

find_package(Boost COMPONENTS log_setup log program_options REQUIRED)

# needs Boost::log Boost::log_setup to overcome the bug in log headers processing by CMake
target_link_libraries(el3dec_netdaemon PRIVATE el3dec_lib ${Boost_LIBRARIES})
target_link_libraries(el3dec_app PRIVATE el3dec_lib)
