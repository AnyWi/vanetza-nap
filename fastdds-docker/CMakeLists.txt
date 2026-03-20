cmake_minimum_required(VERSION 3.10)
project(vanetza_dds_test)

set(CMAKE_CXX_STANDARD 17)

find_package(fastrtps REQUIRED)
find_package(fastcdr REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Optional: find Vanetza if installed
find_package(Vanetza QUIET)

file(GLOB FASTDDS_SRC
    src/fastdds/*.cpp
    src/fastdds/*.hpp
    src/fastdds/*.cxx
)

add_library(fastdds_local
    ${FASTDDS_SRC}
)

target_include_directories(fastdds_local PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fastdds
)

target_link_libraries(fastdds_local
    fastrtps
    fastcdr
    ${Boost_LIBRARIES}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/fastdds)


add_executable(dds_publisher ${FASTDDS_SRC} src/publisher.cpp)
target_link_libraries(dds_publisher
    fastdds_local
)

add_executable(dds_subscriber ${FASTDDS_SRC} src/subscriber.cpp)
target_link_libraries(dds_subscriber
    fastdds_local
    
)

# # Vanetza-integrated test (if Vanetza found)
# if(Vanetza_FOUND)
#     add_executable(vanetza_cam_publisher src/vanetza_cam_publisher.cpp)
#     target_link_libraries(vanetza_cam_publisher
#         Vanetza::vanetza
#         fastrtps
#         fastcdr
#         ${Boost_LIBRARIES}
#     )

#     add_executable(vanetza_cam_subscriber src/vanetza_cam_subscriber.cpp)
#     target_link_libraries(vanetza_cam_subscriber
#         Vanetza::vanetza
#         fastrtps
#         fastcdr
#         ${Boost_LIBRARIES}
#     )
#     message(STATUS "Vanetza found - building integrated examples")
# else()
#     message(STATUS "Vanetza not found - building FastDDS-only examples")
# endif()

install(TARGETS dds_publisher dds_subscriber DESTINATION bin)
