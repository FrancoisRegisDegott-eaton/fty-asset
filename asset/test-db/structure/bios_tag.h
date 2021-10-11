#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createTag(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE `t_bios_tag` (
            `id_tag` int(10) unsigned NOT NULL AUTO_INCREMENT,
            `name` varchar(50) NOT NULL,

            PRIMARY KEY (`id_tag`),
            UNIQUE KEY `UI_t_bios_tag_NAME` (`name`)
        )
    )");

    // clang-format off
    conn.execute(R"(
        CREATE TABLE `t_bios_asset_element_tag_relation` (
            `id_asset_element_tag_relation` int(10) unsigned NOT NULL AUTO_INCREMENT,
            `id_asset_element` int(10) unsigned NOT NULL,
            `id_tag` int(10) unsigned NOT NULL,
            `id_relation_source` int(10) unsigned NOT NULL DEFAULT 0,

            PRIMARY KEY (`id_asset_element_tag_relation`),
            UNIQUE KEY `UI_ASSET_ELEMENT_TAG_RELATION` (`id_asset_element`,`id_tag`),
            KEY `FK_ASSET_ELEMENT_TAG_RELATION_ELEMENT_idx` (`id_asset_element`),
            KEY `FK_ASSET_ELEMENT_TAG_RELATION_TAG_idx` (`id_tag`),
            CONSTRAINT `FK_ASSET_ELEMENT_TAG_RELATION_ELEMENT` FOREIGN KEY (`id_asset_element`) REFERENCES `t_bios_asset_element` (`id_asset_element`) ON DELETE CASCADE ON UPDATE CASCADE,
            CONSTRAINT `FK_ASSET_ELEMENT_TAG_RELATION_TAG` FOREIGN KEY (`id_tag`) REFERENCES `t_bios_tag` (`id_tag`) ON DELETE CASCADE ON UPDATE CASCADE
        )
    )");
    // clang-format on
}

} // namespace fty
