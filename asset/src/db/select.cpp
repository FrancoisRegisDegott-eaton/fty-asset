#include "asset/db/select.h"
#include "asset/error.h"
#include <fty_common_asset_types.h>
#include <fty_common_db_connection.h>
#include <iostream>

namespace fty::db::asset::select {

using namespace fmt::literals;

// =========================================================================================================================================

static fty::Expected<std::string> assetExtSql(const Filter& filter, const Order& order, const std::string& where = {})
{
    static const std::string sql = R"(
        SELECT
            v.id                as id,
            v.name              as name,
            ext.value           as extName,
            v.id_type           as typeId,
            v.type_name         as typeName,
            v.subtype_id        as subTypeId,
            v.subtype_name      as subTypeName,
            v.id_parent         as parentId,
            v.id_parent_type    as parentTypeId,
            v.parent_name       as parentName,
            v.status            as status,
            v.priority          as priority,
            v.asset_tag         as assetTag
        FROM
            v_web_element v
        LEFT JOIN t_bios_asset_ext_attributes AS ext
            ON ext.id_asset_element = v.id AND ext.keytag = "name"
        {orderJoin}
        {where}
        {orderBy}
    )";

    if (order && !order.isValid()) {
        return unexpected(error(fty::asset::Errors::InternalError)
                              .format("order field is invalid, possible orders are '{}'", implode(order.possibleOrders(), "/")));
    }

    std::string filterWhere;
    if (filter) {
        std::vector<std::string> wheres;

        if (!filter.subtypes.empty()) {
            wheres.emplace_back(fmt::format("v.subtype_id in ({})", implode(filter.subtypes, ", ")));
        }
        if (!filter.types.empty()) {
            wheres.emplace_back(fmt::format("v.id_type in ({})", implode(filter.types, ", ")));
        }
        if (!filter.status.empty()) {
            wheres.emplace_back(fmt::format("v.status = '{}'", filter.status));
        }

        if (!filter.without.empty()) {
            if (filter.without == "location") {
                wheres.emplace_back("v.id_parent is NULL");
            } else if (filter.without == "powerchain") {
                std::string pwr = R"(
                    NOT EXISTS
                    (
                        SELECT
                            id_asset_device_dest
                        FROM
                            t_bios_asset_link_type as l
                        JOIN t_bios_asset_link as a
                            ON a.id_asset_link_type=l.id_asset_link_type
                        WHERE
                            name="power chain" AND
                            v.id=a.id_asset_device_dest
                    )
                )";
                wheres.emplace_back(pwr);
            } else {
                auto other = fmt::format(
                    R"(
                    NOT EXISTS
                    (
                        SELECT a.id_asset_element
                        FROM
                            t_bios_asset_ext_attributes as a
                        WHERE
                            a.keytag="{}" AND v.id = a.id_asset_element
                    )
                )",
                    filter.without);
                wheres.emplace_back(other);
            }
        }

        filterWhere = implode(wheres, " AND ");
    }
    std::string orderJoin;
    if (order) {
        orderJoin = R"(
            LEFT JOIN t_bios_asset_ext_attributes orderAttr
                ON id = orderAttr.id_asset_element AND orderAttr.keytag = '{}'
        )"_format(order.field);
    }

    std::string orderBy;
    if (order) {
        orderBy = "ORDER BY {0} {1}, id {1}"_format(
            order.dir == Order::Dir::Asc ? "COALESCE (orderAttr.value, 'ZZZZZZ999999')" : "orderAttr.value",
            order.dir == Order::Dir::Asc ? "ASC" : "DESC");
    }

    std::string wstr;
    if (where.empty() && !filterWhere.empty()) {
        wstr = "WHERE {}"_format(filterWhere);
    } else if (!where.empty() && !filterWhere.empty()) {
        wstr = "{} AND {}"_format(where, filterWhere);
    } else {
        wstr = where;
    }

    return fmt::format(sql, "orderJoin"_a = orderJoin, "where"_a = wstr, "orderBy"_a = orderBy);
}

static void fetchAssetExt(const Row& row, AssetItemExt& asset)
{
    row.get("id", asset.id);
    row.get("name", asset.name);
    row.get("extName", asset.extName);
    row.get("typeId", asset.typeId);
    row.get("typeName", asset.typeName);
    row.get("subTypeId", asset.subtypeId);
    row.get("subTypeName", asset.subtypeName);
    row.get("parentId", asset.parentId);
    row.get("parentTypeId", asset.parentTypeId);
    row.get("parentName", asset.parentName);
    row.get("status", asset.status);
    row.get("priority", asset.priority);
    row.get("assetTag", asset.assetTag);
}

// =========================================================================================================================================

Filter::operator bool() const
{
    return !subtypes.empty() || !types.empty() || !status.empty() || !without.empty();
}

// =========================================================================================================================================

const std::set<std::string>& Order::possibleOrders() const
{
    static const std::set<std::string> orders = {"name",      "model",     "create_ts", "firmware",
                                                 "max_power", "serial_no", "update_ts", "asset_order"};
    return orders;
}

Order::operator bool() const
{
    return !field.empty();
}

bool Order::isValid() const
{
    return possibleOrders().find(field) != possibleOrders().end();
}

// =========================================================================================================================================
// Select basic information about asset element by name
// =========================================================================================================================================

Expected<AssetItem> item(const std::string& elementName, bool extNameOnly)
{
    try {
        Connection db;
        return item(db, elementName, extNameOnly);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementName));
    }
}

Expected<AssetItem> item(Connection& conn, const std::string& elementName, bool extNameOnly)
{
    static const std::string nameSql = R"(
        SELECT
            v.name,
            v.id_parent,
            v.status,
            v.priority,
            v.id,
            v.id_type,
            v.id_subtype
        FROM
            v_bios_asset_element v
        WHERE
            v.name = :name
    )";

    static const std::string extNameSql = R"(
        SELECT
            v.name,
            v.id_parent,
            v.status,
            v.priority,
            v.id,
            v.id_type,
            v.id_subtype
        FROM
            v_bios_asset_element AS v
        LEFT JOIN v_bios_asset_ext_attributes AS ext
            ON ext.id_asset_element = v.id
        WHERE
            ext.keytag = 'name' AND ext.value = :name
    )";

    if (!persist::is_ok_name(elementName.c_str())) {
        return unexpected("name '{}' is not valid"_tr, elementName);
    }

    try {
        Row row;

        if (extNameOnly) {
            row = conn.selectRow(extNameSql, "name"_p = elementName);
        } else {
            try {
                row = conn.selectRow(nameSql, "name"_p = elementName);
            } catch (const NotFound&) {
                row = conn.selectRow(extNameSql, "name"_p = elementName);
            }
        }

        AssetItem el;
        // clang-format off
        row.get("name",       el.name);
        row.get("id_parent",  el.parentId);
        row.get("status",     el.status);
        row.get("priority",   el.priority);
        row.get("id",         el.id);
        row.get("id_type",    el.typeId);
        row.get("id_subtype", el.subtypeId);
        // clang-format on

        return std::move(el);
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(elementName));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementName));
    }
}

// =========================================================================================================================================
// Selects all assets
// =========================================================================================================================================

Expected<void> items(Callback&& cb, const Filter& filter, const Order& order)
{
    try {
        Connection db;
        return items(db, std::move(cb), filter, order);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

Expected<void> items(Connection& conn, Callback&& cb, const Filter& filter, const Order& order)
{
    auto sql = assetExtSql(filter, order);
    if (!sql) {
        return unexpected(sql.error());
    }

    try {
        for (auto& row : conn.select(*sql)) {
            cb(row);
        }
        return {};
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

// =========================================================================================================================================
// Selects all assets
// =========================================================================================================================================

Expected<std::vector<AssetItemExt>> items(const Filter& filter, const Order& order)
{
    try {
        Connection conn;
        return items(conn, filter, order);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

Expected<std::vector<AssetItemExt>> items(Connection& conn, const Filter& filter, const Order& order)
{
    auto sql = assetExtSql(filter, order);
    if (!sql) {
        return unexpected(sql.error());
    }

    try {
        std::vector<AssetItemExt> res;
        for (auto& row : conn.select(*sql)) {
            AssetItemExt el;
            fetchAssetExt(row, el);
            res.emplace_back(std::move(el));
        }
        return res;
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

// =========================================================================================================================================
// Selects all data about asset in AssetItemEx
// =========================================================================================================================================

Expected<AssetItemExt> itemExt(uint32_t elementId)
{
    AssetItemExt el;

    if (auto ret = itemExt(elementId, el)) {
        return std::move(el);
    } else {
        return unexpected(ret.error());
    }
}

Expected<AssetItemExt> itemExt(Connection& conn, uint32_t elementId)
{
    AssetItemExt el;

    if (auto ret = itemExt(conn, elementId, el)) {
        return std::move(el);
    } else {
        return unexpected(ret.error());
    }
}

// =========================================================================================================================================
// Selects all data about asset in AssetItemEx
// =========================================================================================================================================

Expected<void> itemExt(uint32_t elementId, AssetItemExt& asset)
{
    try {
        Connection db;
        return itemExt(db, elementId, asset);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementId));
    }
}

Expected<void> itemExt(Connection& conn, uint32_t elementId, AssetItemExt& asset)
{
    auto sql = assetExtSql({}, {}, "WHERE :id = v.id");
    if (!sql) {
        return unexpected(sql.error());
    }

    try {
        auto row = conn.selectRow(*sql, "id"_p = elementId);
        fetchAssetExt(row, asset);
        return {};
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(elementId));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementId));
    }
}

// =========================================================================================================================================
// Selects all data about asset by name
// =========================================================================================================================================

Expected<AssetItemExt> itemExt(const std::string& name)
{
    try {
        Connection db;
        return itemExt(db, name);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), name));
    }
}

Expected<AssetItemExt> itemExt(Connection& conn, const std::string& name)
{
    if (!persist::is_ok_name(name.c_str())) {
        return unexpected("name '{}' is not valid"_tr, name);
    }

    auto sql = assetExtSql({}, {}, "WHERE :name = v.name");
    if (!sql) {
        return unexpected(sql.error());
    }

    try {
        auto         row = conn.selectRow(*sql, "name"_p = name);
        AssetItemExt asset;
        fetchAssetExt(row, asset);
        return asset;
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(name));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), name));
    }
}

// =========================================================================================================================================
// Selects assets from given container
// =========================================================================================================================================

Expected<void> itemsByContainer(uint32_t containerId, Callback&& cb, const Filter& filter, const Order& order)
{
    try {
        Connection db;
        return itemsByContainer(db, containerId, std::move(cb), filter, order);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), containerId));
    }
}

Expected<void> itemsByContainer(Connection& conn, uint32_t containerId, Callback&& cb, const Filter& filter, const Order& order)
{
    static const std::string where = R"(
        WHERE v.id in (
            SELECT
                sp.id_asset_element
            FROM
                v_bios_asset_element_super_parent AS sp
            WHERE
                :containerid in (
                    sp.id_parent1, sp.id_parent2, sp.id_parent3,
                    sp.id_parent4, sp.id_parent5, sp.id_parent6,
                    sp.id_parent7, sp.id_parent8, sp.id_parent9,
                    sp.id_parent10
                )
        )
    )";

    auto sql = assetExtSql(filter, order, where);
    if (!sql) {
        return unexpected(sql.error());
    }
    try {
        for (const auto& row : conn.select(*sql, "containerid"_p = containerId)) {
            cb(row);
        }
        return {};
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), containerId));
    }
}

// =========================================================================================================================================
// Selects assets from given container
// =========================================================================================================================================

Expected<std::vector<AssetItemExt>> itemsByContainer(uint32_t containerId, const Filter& flt, const Order& order)
{
    try {
        Connection conn;
        return itemsByContainer(conn, containerId, flt, order);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), containerId));
    }
}

Expected<std::vector<AssetItemExt>> itemsByContainer(Connection& conn, uint32_t containerId, const Filter& flt, const Order& order)
{
    try {
        std::vector<AssetItemExt> result;

        auto func = [&](const fty::db::Row& row) {
            AssetItemExt el;
            fetchAssetExt(row, el);
            result.emplace_back(std::move(el));
        };

        if (auto res = itemsByContainer(conn, containerId, func, flt, order); !res) {
            return unexpected(res.error());
        }
        return result;
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), containerId));
    }
}

// =========================================================================================================================================
// Select all assets in all (or without) containers
// =========================================================================================================================================

Expected<void> itemsWithoutContainer(Callback&& cb, const Filter& filter, const Order& order)
{
    try {
        Connection conn;
        return itemsWithoutContainer(conn, std::move(cb), filter, order);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

Expected<void> itemsWithoutContainer(Connection& conn, Callback&& cb, const Filter& filter, const Order& order)
{
    auto sql = assetExtSql(filter, order, "WHERE v.id_parent is NULL");
    if (!sql) {
        return unexpected(sql.error());
    }

    try {
        for (const auto& row : conn.select(*sql)) {
            cb(row);
        }
        return {};
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

// =========================================================================================================================================
// Select all assets in all (or without) containers
// =========================================================================================================================================

Expected<std::vector<AssetItemExt>> itemsWithoutContainer(const Filter& filter, const Order& order)
{
    try {
        Connection conn;
        return itemsWithoutContainer(conn, filter, order);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

Expected<std::vector<AssetItemExt>> itemsWithoutContainer(Connection& conn, const Filter& filter, const Order& order)
{
    try {
        std::vector<AssetItemExt> result;

        auto func = [&](const fty::db::Row& row) {
            AssetItemExt el;
            fetchAssetExt(row, el);
            result.emplace_back(std::move(el));
        };

        if (auto res = itemsWithoutContainer(conn, func, filter, order); !res) {
            return unexpected(res.error());
        }
        return result;
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

// =========================================================================================================================================
// Selects all ext_attributes of asset
// =========================================================================================================================================

Expected<Attributes> extAttributes(uint32_t elementId)
{
    try {
        Connection db;
        return extAttributes(db, elementId);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementId));
    }
}

Expected<Attributes> extAttributes(Connection& conn, uint32_t elementId)
{
    static const std::string sql = R"(
        SELECT
            v.keytag,
            v.value,
            v.read_only
        FROM
            v_bios_asset_ext_attributes v
        WHERE
            v.id_asset_element = :elementId
    )";

    try {
        auto result = conn.select(sql, "elementId"_p = elementId);

        Attributes attrs;

        for (const auto& row : result) {
            ExtAttrValue val;

            row.get("value", val.value);
            row.get("read_only", val.readOnly);

            attrs.emplace(row.get("keytag"), val);
        }

        return std::move(attrs);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementId));
    }
}

// =========================================================================================================================================
// Selects all ext_attributes of asset
// =========================================================================================================================================

Expected<std::vector<AssetLink>> deviceLinksTo(uint32_t elementId, uint8_t linkTypeId)
{
    try {
        fty::db::Connection conn;
        return deviceLinksTo(conn, elementId, linkTypeId);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementId));
    }
}

Expected<std::vector<AssetLink>> deviceLinksTo(Connection& conn, uint32_t elementId, uint8_t linkTypeId)
{
    static const std::string sql = R"(
        SELECT
            v.id_asset_element_src,
            v.src_out,
            v.dest_in,
            v.src_name
        FROM
            v_web_asset_link v
        WHERE
            v.id_asset_element_dest = :iddevice AND
            v.id_asset_link_type = :idlinktype
    )";


    try {
        // clang-format off
        auto rows = conn.select(sql,
            "iddevice"_p   = elementId,
            "idlinktype"_p = linkTypeId
        );
        // clang-format on

        std::vector<AssetLink> ret;

        for (const auto& row : rows) {
            AssetLink link;
            link.destId = elementId;
            row.get("id_asset_element_src", link.srcId);
            row.get("src_out", link.srcSocket);
            row.get("dest_in", link.destSocket);
            row.get("src_name", link.srcName);

            ret.push_back(link);
        }
        return std::move(ret);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), elementId));
    }
}
} // namespace fty::db::asset::select
