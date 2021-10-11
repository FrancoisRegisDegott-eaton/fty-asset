#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createDeviceType(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_device_type(
            id_device_type      TINYINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name                VARCHAR(50)      NOT NULL,

            PRIMARY KEY(id_device_type),

            UNIQUE INDEX `UI_t_bios_device_type_name` (`name` ASC)

        ) AUTO_INCREMENT = 1;
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_device_type AS
            SELECT id_device_type id, name FROM t_bios_device_type;
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_device AS
            SELECT  t1.id_asset_element,
                    t2.id_asset_device_type,
                    t2.name
            FROM t_bios_asset_element t1
                LEFT JOIN t_bios_asset_device_type t2
                ON (t1.id_subtype = t2.id_asset_device_type);
    )");

    conn.execute(R"(
        INSERT INTO t_bios_device_type (name) VALUES
            ("not_classified"),
            ("ups"),
            ("epdu"),
            ("server");
    )");
}

}
