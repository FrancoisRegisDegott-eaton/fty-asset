#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createDiscoveryDevice(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_discovered_device(
            id_discovered_device    SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name                    VARCHAR(50)       NOT NULL,
            id_device_type          TINYINT UNSIGNED  NOT NULL,

            PRIMARY KEY(id_discovered_device),

            INDEX(id_device_type),

            UNIQUE INDEX `UI_t_bios_discovered_device_name` (`name` ASC),

            FOREIGN KEY(id_device_type)
            REFERENCES t_bios_device_type(id_device_type)
                ON DELETE RESTRICT
        );
    )");
}

} // namespace fty
