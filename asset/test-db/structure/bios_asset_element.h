#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createAssetElement(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_element (
            id_asset_element  INT UNSIGNED        NOT NULL AUTO_INCREMENT,
            name              VARCHAR(50)         NOT NULL,
            id_type           TINYINT UNSIGNED    NOT NULL,
            id_subtype        TINYINT UNSIGNED    NOT NULL DEFAULT 11,
            id_parent         INT UNSIGNED,
            status            VARCHAR(9)          NOT NULL DEFAULT "nonactive",
            priority          TINYINT             NOT NULL DEFAULT 5,
            asset_tag         VARCHAR(50),

            PRIMARY KEY (id_asset_element),

            INDEX FK_ASSETELEMENT_ELEMENTTYPE_idx (id_type   ASC),
            INDEX FK_ASSETELEMENT_ELEMENTSUBTYPE_idx (id_subtype   ASC),
            INDEX FK_ASSETELEMENT_PARENTID_idx    (id_parent ASC),
            UNIQUE INDEX `UI_t_bios_asset_element_NAME` (`name` ASC),
            INDEX `UI_t_bios_asset_element_ASSET_TAG` (`asset_tag`  ASC),

            CONSTRAINT FK_ASSETELEMENT_ELEMENTTYPE
            FOREIGN KEY (id_type)
            REFERENCES t_bios_asset_element_type (id_asset_element_type)
            ON DELETE RESTRICT,

            CONSTRAINT FK_ASSETELEMENT_ELEMENTSUBTYPE
            FOREIGN KEY (id_subtype)
            REFERENCES t_bios_asset_device_type (id_asset_device_type)
            ON DELETE RESTRICT,

            CONSTRAINT FK_ASSETELEMENT_PARENTID
            FOREIGN KEY (id_parent)
            REFERENCES t_bios_asset_element (id_asset_element)
            ON DELETE RESTRICT
        );
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_element AS
            SELECT  v1.id_asset_element AS id,
                    v1.name,
                    v1.id_type,
                    v1.id_subtype,
                    v1.id_parent,
                    v2.id_type AS id_parent_type,
                    v2.name AS parent_name,
                    v1.status,
                    v1.priority,
                    v1.asset_tag
                FROM t_bios_asset_element v1
                LEFT JOIN  t_bios_asset_element v2
                    ON (v1.id_parent = v2.id_asset_element) ;
    )");

    conn.execute(R"(
        CREATE VIEW v_web_element AS
            SELECT
                t1.id_asset_element AS id,
                t1.name,
                t1.id_type,
                v3.name AS type_name,
                t1.id_subtype AS subtype_id,
                v4.name AS subtype_name,
                t1.id_parent,
                t2.id_type AS id_parent_type,
                t2.name AS parent_name,
                t1.status,
                t1.priority,
                t1.asset_tag
            FROM
                t_bios_asset_element t1
                LEFT JOIN t_bios_asset_element t2
                    ON (t1.id_parent = t2.id_asset_element)
                LEFT JOIN v_bios_asset_element_type v3
                    ON (t1.id_type = v3.id)
                LEFT JOIN t_bios_asset_device_type v4
                    ON (v4.id_asset_device_type = t1.id_subtype);
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_element_super_parent AS
        SELECT v1.id as id_asset_element,
               v1.id_parent AS id_parent1,
               v2.id_parent AS id_parent2,
               v3.id_parent AS id_parent3,
               v4.id_parent AS id_parent4,
               v5.id_parent AS id_parent5,
               v6.id_parent AS id_parent6,
               v7.id_parent AS id_parent7,
               v8.id_parent AS id_parent8,
               v9.id_parent AS id_parent9,
               v10.id_parent AS id_parent10,
               v1.parent_name AS name_parent1,
               v2.parent_name AS name_parent2,
               v3.parent_name AS name_parent3,
               v4.parent_name AS name_parent4,
               v5.parent_name AS name_parent5,
               v6.parent_name AS name_parent6,
               v7.parent_name AS name_parent7,
               v8.parent_name AS name_parent8,
               v9.parent_name AS name_parent9,
               v10.parent_name AS name_parent10,
               v2.id_type AS id_type_parent1,
               v3.id_type AS id_type_parent2,
               v4.id_type AS id_type_parent3,
               v5.id_type AS id_type_parent4,
               v6.id_type AS id_type_parent5,
               v7.id_type AS id_type_parent6,
               v8.id_type AS id_type_parent7,
               v9.id_type AS id_type_parent8,
               v10.id_type AS id_type_parent9,
               v11.id_type AS id_type_parent10,
               v2.id_subtype AS id_subtype_parent1,
               v3.id_subtype AS id_subtype_parent2,
               v4.id_subtype AS id_subtype_parent3,
               v5.id_subtype AS id_subtype_parent4,
               v6.id_subtype AS id_subtype_parent5,
               v7.id_subtype AS id_subtype_parent6,
               v8.id_subtype AS id_subtype_parent7,
               v9.id_subtype AS id_subtype_parent8,
               v10.id_subtype AS id_subtype_parent9,
               v11.id_subtype AS id_subtype_parent10,
               v1.name ,
               v12.name AS type_name,
               v12.id_asset_device_type,
               v1.status,
               v1.asset_tag,
               v1.priority,
               v1.id_type
        FROM v_bios_asset_element v1
             LEFT JOIN v_bios_asset_element v2
                ON (v1.id_parent = v2.id)
             LEFT JOIN v_bios_asset_element v3
                ON (v2.id_parent = v3.id)
             LEFT JOIN v_bios_asset_element v4
                ON (v3.id_parent = v4.id)
             LEFT JOIN v_bios_asset_element v5
                ON (v4.id_parent = v5.id)
             LEFT JOIN v_bios_asset_element v6
                ON (v5.id_parent = v6.id)
             LEFT JOIN v_bios_asset_element v7
                ON (v6.id_parent = v7.id)
             LEFT JOIN v_bios_asset_element v8
                ON (v7.id_parent = v8.id)
             LEFT JOIN v_bios_asset_element v9
                ON (v8.id_parent = v9.id)
             LEFT JOIN v_bios_asset_element v10
                ON (v9.id_parent = v10.id)
             LEFT JOIN v_bios_asset_element v11
                ON (v10.id_parent = v11.id)
             INNER JOIN t_bios_asset_device_type v12
                ON (v12.id_asset_device_type = v1.id_subtype);
    )");
}

}
