#pragma once
#include <fty_common_db_connection.h>

namespace fty {

void createAssetLinkType(fty::db::Connection& conn)
{
    conn.execute(R"(
        CREATE TABLE t_bios_asset_link_type(
          id_asset_link_type   TINYINT UNSIGNED   NOT NULL AUTO_INCREMENT,
          name                 VARCHAR(50)        NOT NULL,

          PRIMARY KEY (id_asset_link_type),
          UNIQUE INDEX `UI_t_bios_asset_link_type_name` (`name` ASC)

        );
    )");

    conn.execute(R"(
        INSERT INTO t_bios_asset_link_type (id_asset_link_type, name)
        VALUES  (1, "power chain"),
                (2, "vmware.vcenter.monitors.esxi"),
                (3, "vmware.cluster.contains.esxi"),
                (4, "vmware.esxi.hosts.vm"),
                (5, "vmware.standalone.esxi.hosts.vm"),
                (6, "vmware.vcenter.monitors.cluster"),
                (7, "vmware.vcenter.monitors.vapp"),
                (8, "citrix.zenserver.hosts.vm"),
                (9, "citrix.pool.monitors.xenserver"),
                (10, "citrix.pool.monitors.vapp"),
                (11, "citrix.pool.monitors.task"),
                (12, "citrix.pool.monitors.halted.vm"),
                (13, "hp.it.rack.link.server"),
                (14, "hp.it.manager.monitor.server"),
                (15, "hp.it.manager.monitor.rack"),
                (16, "microsoft.hyperv.hosts.vm"),
                (17, "netapp.cluster.contains.node"),
                (18, "nutanix.cluster.contains.node"),
                (19, "nutanix.node.hosts.vm"),
                (20, "nutanix.proxy.monitors.cluster"),
                (21, "nutanix.proxy.monitors.node"),
                (22, "nutanix.proxy.monitors.vm"),
                (23, "ipminfra.server.hosts.os"),
                (24, "nutanix.connected.to.prism"),
                (25, "microsoft.connected.to.server"),
                (26, "hp.it.connected.to.oneview"),
                (27, "vmware.connected.to.vcenter"),
                (28, "vmware.connected.to.esxi"),
                (29, "netapp.connected.to.ontap"),
                (30, "netapp.ontap.monitor.cluster"),
                (31, "vmware.vcenter.manages.srm"),
                (32, "vmware.srm.has.plan"),
                (33, "server.hosts.hypervisor"),
                (35, "dell.vxrail.connected.to.manager"),
                (36, "dell.vxrail.connected.to.cluster"),
                (37, "microsoft.server.has.hyperv.service"),
                (38, "microsoft.scvmm.has.hyperv.server"),
                (40, "vmware.cluster.contains.fault.domain"),
                (41, "vmware.cluster.fault.domain.contains.esxi"),
                (42, "microsoft.connected.to.scvmm"),
                (43, "microsoft.scvmm.has.cluster"),
                (44, "microsoft.cluster.hosts.hyperv.server");
    )");
}

}
