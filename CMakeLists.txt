cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

project(fty-asset
    VERSION 1.0.0
    DESCRIPTION "Asset management DTO, library and agent"
)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

# libfty-asset.so (CRUD asset management)
add_subdirectory(lib)

# libfty-asset-libng.so (CRUD asset management -new generation-)
add_subdirectory(asset)

# libfty-asset-accessor.so (client/server messenger)
add_subdirectory(accessor)

# fty-asset-server
add_subdirectory(server)

