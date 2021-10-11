#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createAssetLink(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_link (
          id_link               INT UNSIGNED        NOT NULL AUTO_INCREMENT,
          id_asset_device_src   INT UNSIGNED        NOT NULL,
          src_out               CHAR(4),
          id_asset_device_dest  INT UNSIGNED        NOT NULL,
          dest_in               CHAR(4),
          id_asset_link_type    TINYINT UNSIGNED    NOT NULL,

          PRIMARY KEY (id_link),

          INDEX FK_ASSETLINK_SRC_idx  (id_asset_device_src    ASC),
          INDEX FK_ASSETLINK_DEST_idx (id_asset_device_dest   ASC),
          INDEX FK_ASSETLINK_TYPE_idx (id_asset_link_type     ASC),

          CONSTRAINT FK_ASSETLINK_SRC
            FOREIGN KEY (id_asset_device_src)
            REFERENCES t_bios_asset_element(id_asset_element)
            ON DELETE RESTRICT,

          CONSTRAINT FK_ASSETLINK_DEST
            FOREIGN KEY (id_asset_device_dest)
            REFERENCES t_bios_asset_element(id_asset_element)
            ON DELETE RESTRICT,

          CONSTRAINT FK_ASSETLINK_TYPE
            FOREIGN KEY (id_asset_link_type)
            REFERENCES t_bios_asset_link_type(id_asset_link_type)
            ON DELETE RESTRICT

        );
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_link AS
            SELECT  v1.id_link,
                    v1.src_out,
                    v1.dest_in,
                    v1.id_asset_link_type,
                    v1.id_asset_device_src AS id_asset_element_src,
                    v1.id_asset_device_dest AS id_asset_element_dest
            FROM t_bios_asset_link v1;
    )");

    conn.execute(R"(
        CREATE VIEW v_web_asset_link AS
            SELECT
                v1.id_link,
                v1.id_asset_link_type,
                t3.name AS link_name,
                v1.id_asset_element_src,
                t1.name AS src_name,
                v1.id_asset_element_dest,
                t2.name AS dest_name,
                v1.src_out,
                v1.dest_in
            FROM
                 v_bios_asset_link v1
            JOIN t_bios_asset_element t1
                ON v1.id_asset_element_src=t1.id_asset_element
            JOIN t_bios_asset_element t2
                ON v1.id_asset_element_dest=t2.id_asset_element
            JOIN t_bios_asset_link_type t3
                ON v1.id_asset_link_type=t3.id_asset_link_type;
    )");

}

}
