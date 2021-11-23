cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

project(fty-asset
    VERSION 1.0.0
    DESCRIPTION "Asset management DTO, library and agent"
)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################


##############################################################################################################
add_subdirectory(lib)
add_subdirectory(accessor)
add_subdirectory(asset)
add_subdirectory(server)
##############################################################################################################
