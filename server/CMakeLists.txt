cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################

file(GLOB_RECURSE SOURCES_FILES src/*.cc)

etn_target(exe ${PROJECT_NAME}-server
    SOURCES
        ${SOURCES_FILES}

    INCLUDE_DIRS
        src
        src/topology
        src/topology/db
        src/topology/msg
        src/topology/persist
        src/topology/shared
        src/asset
        src/asset/conversion
        include
    USES_PUBLIC
        fty_common
    USES_PRIVATE
        ${PROJECT_NAME}
        ${PROJECT_NAME}-libng
        cxxtools
        fty_common
        fty_common_db
        fty_common_logging
        fty_proto
        fty_common_mlm
        fty_common_dto
        fty_common_messagebus
        fty_common_socket
        fty_security_wallet
        fty-utils
        tntdb
        czmq
        mlm
        crypto
        protobuf
        uuid
)

#install stystemd config

configure_file(fty-asset.service.in
  "${PROJECT_BINARY_DIR}/fty-asset.service" @ONLY)

install(FILES
  "${PROJECT_BINARY_DIR}/fty-asset.service"
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/
)


##############################################################################################################

