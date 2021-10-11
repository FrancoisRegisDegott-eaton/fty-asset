#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createGroupRelations(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_group_relation (
          id_asset_group_relation INT UNSIGNED NOT NULL AUTO_INCREMENT,
          id_asset_group          INT UNSIGNED NOT NULL,
          id_asset_element        INT UNSIGNED NOT NULL,

          PRIMARY KEY (id_asset_group_relation),

          INDEX FK_ASSETGROUPRELATION_ELEMENT_idx (id_asset_element ASC),
          INDEX FK_ASSETGROUPRELATION_GROUP_idx   (id_asset_group   ASC),

          UNIQUE INDEX `UI_t_bios_asset_group_relation` (`id_asset_group`, `id_asset_element` ASC),

          CONSTRAINT FK_ASSETGROUPRELATION_ELEMENT
            FOREIGN KEY (id_asset_element)
            REFERENCES t_bios_asset_element (id_asset_element)
            ON DELETE RESTRICT,

          CONSTRAINT FK_ASSETGROUPRELATION_GROUP
            FOREIGN KEY (id_asset_group)
            REFERENCES t_bios_asset_element (id_asset_element)
            ON DELETE RESTRICT
        );
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_group_relation AS
            SELECT * FROM t_bios_asset_group_relation;
    )");
}

} // namespace fty
