#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createAssetDeviceType(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_device_type(
            id_asset_device_type TINYINT UNSIGNED   NOT NULL AUTO_INCREMENT,
            name                 VARCHAR(50)        NOT NULL,
            PRIMARY KEY (id_asset_device_type),
            UNIQUE INDEX `UI_t_bios_asset_device_type` (`name` ASC)
        );
    )");

    conn.execute(R"(
        CREATE VIEW v_bios_asset_device_type AS
            SELECT id_asset_device_type as id, name FROM t_bios_asset_device_type;
    )");

    conn.execute(R"(
        INSERT INTO t_bios_asset_device_type (id_asset_device_type, name)
        VALUES  (1, "ups"),
                (2, "genset"),
                (3, "epdu"),
                (4, "pdu"),
                (5, "server"),
                (6, "feed"),
                (7, "sts"),
                (8, "switch"),
                (9, "storage"),
                (10, "vm"),
                (11, "N_A"),
                (12, "router"),
                (13, "rack controller"),
                (14, "sensor"),
                (15, "appliance"),
                (16, "chassis"),
                (17, "patch panel"),
                (18, "other"),
                (19, "sensorgpio"),
                (20, "gpo"),
                (21, "netapp.ontap.node"),
                (22, "ipminfra.server"),
                (23, "ipminfra.service"),
                (24, "vmware.vcenter"),
                (25, "citrix.pool"),
                (26, "vmware.cluster"),
                (27, "vmware.esxi"),
                (28, "microsoft.hyperv.server"),
                (29, "vmware.vm"),
                (31, "citrix.vm"),
                (32, "netapp.node"),
                (33, "vmware.standalone.esxi"),
                (34, "vmware.task"),
                (35, "vmware.vapp"),
                (36, "citrix.xenserver"),
                (37, "citrix.vapp"),
                (38, "citrix.task"),
                (39, "microsoft.vm"),
                (40, "microsoft.task"),
                (41, "microsoft.server.connector"),
                (42, "microsoft.server"),
                (43, "microsoft.cluster"),
                (44, "hp.oneview.connector"),
                (45, "hp.oneview"),
                (46, "hp.it.server"),
                (47, "hp.it.rack"),
                (48, "netapp.server"),
                (49, "netapp.ontap.connector"),
                (50, "netapp.ontap.cluster"),
                (51, "nutanix.vm"),
                (52, "nutanix.prism.gateway"),
                (53, "nutanix.node"),
                (54, "nutanix.cluster"),
                (55, "nutanix.prism.connector"),
                (60, "vmware.vcenter.connector"),
                (61, "vmware.standalone.esxi.connector"),
                (62, "netapp.ontap"),
                (65, "vmware.srm"),
                (66, "vmware.srm.plan"),
                (67, "pcu"),
                (68, "dell.vxrail.connector"),
                (69, "dell.vxrail.manager"),
                (70, "dell.vxrail.cluster"),
                (72, "microsoft.hyperv.service"),
                (73, "vmware.cluster.fault.domain"),
                (74, "microsoft.scvmm.connector"),
                (75, "microsoft.scvmm");
    )");
}

} // namespace fty
