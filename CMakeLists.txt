# Copyright 2020 USC DBLab. All rights reserved.

cmake_minimum_required(VERSION 3.9)
# Keep the version below in sync with the one in db.h
project(rdmalib VERSION 1.22.0 LANGUAGES C CXX)
## use C++11 features
add_definitions(-std=c++11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
# set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_COMPILER /usr/bin/c++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
# CMAKE_CXX_COMPILER= /usr/bin/c++

# mianlusc: https://fmt.dev/latest/usage.html#building
add_subdirectory(fmt)

# mianlusc: https://gflags.github.io/gflags/#cmake
add_subdirectory(gflags)


add_library(rdmalib "" )
target_sources(rdmalib
        PRIVATE
        nova/common.hpp
        nova/logging.hpp
        nova/mr.hpp
        nova/msg_interface.hpp
        nova/pre_connector.hpp
        nova/qp.hpp
        nova/qp_impl.hpp
        nova/rdma_ctrl.hpp
        nova/rdma_ctrl_impl.hpp
        nova/rnic.hpp
        nova/nova_config.h
        nova/nova_config.cc
        nova/nova_common.cpp
        nova/nova_common.h
        nova/nova_msg_callback.h
        nova/nova_rdma_rc_broker.cpp
        nova/nova_rdma_rc_broker.h
        nova/nova_rdma_broker.h
        nova/nova_mem_manager.cpp
        nova/nova_mem_manager.h
        )
# Needed by port_stdcxx.h
find_package(Threads REQUIRED)
target_link_libraries(rdmalib Threads::Threads -lpthread)
target_link_libraries(rdmalib ibverbs fmt)

# add_executable(example_main "nova/example_main.cpp")
add_executable(nova_p2_main "nova/nova_p2_main.cpp")
target_link_libraries(nova_p2_main gflags::gflags)
# target_link_libraries(example_main -lgflags rdmalib)
target_link_libraries(nova_p2_main rdmalib)
