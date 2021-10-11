#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createAssetAttributes(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_ext_attributes(
            id_asset_ext_attribute    INT UNSIGNED NOT NULL AUTO_INCREMENT,
            keytag                    VARCHAR(40)  NOT NULL,
            value                     VARCHAR(255) NOT NULL,
            id_asset_element          INT UNSIGNED NOT NULL,
            read_only                 TINYINT      NOT NULL DEFAULT 0,

            PRIMARY KEY (id_asset_ext_attribute),

            INDEX FK_ASSETEXTATTR_ELEMENT_idx (id_asset_element ASC),
            UNIQUE INDEX `UI_t_bios_asset_ext_attributes` (`keytag`, `id_asset_element` ASC),

            CONSTRAINT FK_ASSETEXTATTR_ELEMENT
            FOREIGN KEY (id_asset_element)
            REFERENCES t_bios_asset_element (id_asset_element)
            ON DELETE CASCADE
        );
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_ext_attributes AS
            SELECT * FROM t_bios_asset_ext_attributes ;
    )");
}

} // namespace fty
