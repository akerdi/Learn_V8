cmake_minimum_required(VERSION 3.16.3)

project(app)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX -Wl,--no-as-needed")

aux_source_directory(. SRC_DIRS)

add_executable(app ${SRC_DIRS})

target_include_directories(app PUBLIC /home/aker/learn_cpp/v8/v8/include)
target_link_directories(app PUBLIC /home/aker/learn_cpp/v8/v8/out.gn/x64.release.sample/obj)

target_link_libraries(app -ldl -lpthread -lv8_monolith -lv8_libbase -lv8_libplatform)
