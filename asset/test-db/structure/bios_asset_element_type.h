#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createElementType(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_element_type (
            id_asset_element_type TINYINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name                  VARCHAR(50)      NOT NULL,
            PRIMARY KEY (id_asset_element_type),
            UNIQUE INDEX `UI_t_bios_asset_element_type` (`name` ASC)
        ) AUTO_INCREMENT = 1;
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_element_type AS
            SELECT
                t1.id_asset_element_type AS id,
                t1.name
            FROM
                t_bios_asset_element_type t1;
    )");

    conn.execute(R"(
        INSERT INTO t_bios_asset_element_type (id_asset_element_type, name)
        VALUES  (1,  "group"),
                (2,  "datacenter"),
                (3,  "room"),
                (4,  "row"),
                (5,  "rack"),
                (6,  "device"),
                (7,  "infra-service"),
                (8,  "cluster"),
                (9,  "hypervisor"),
                (10, "virtual-machine"),
                (11, "storage-service"),
                (12, "vapp"),
                (13, "connector"),
                (15, "server"),
                (16, "planner"),
                (17, "plan"),
                (18, "cops"),
                (19, "operating-system"),
                (20, "host-group"),
                (21, "container-cluster"),
                (22, "container-node");
    )");
}

}
