#include "asset/db/misc.h"
#include "asset/error.h"
#include <fty_common_asset_types.h>
#include <fty_common_db_connection.h>

namespace fty::db::asset {

// =========================================================================================================================================
// Converts asset internal name to database id
// =========================================================================================================================================

Expected<uint32_t> nameToAssetId(const std::string& assetName)
{
    try {
        Connection db;
        return nameToAssetId(db, assetName);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetName));
    }
}

Expected<uint32_t> nameToAssetId(Connection& conn, const std::string& assetName)
{
    static const std::string sql = R"(
        SELECT
            id_asset_element
        FROM
            t_bios_asset_element
        WHERE name = :assetName
    )";

    if (!persist::is_ok_name(assetName.c_str())) {
        return unexpected("'{}' name is not valid"_tr, assetName);
    }

    try {
        auto res = conn.selectRow(sql, "assetName"_p = assetName);
        return res.get<uint32_t>("id_asset_element");
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(assetName));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetName));
    }
}

// =========================================================================================================================================
// Converts database id to internal name and extended (unicode) name
// =========================================================================================================================================

Expected<Names> idToNameExtName(uint32_t assetId)
{
    try {
        Connection db;
        return idToNameExtName(db, assetId);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetId));
    }
}

Expected<Names> idToNameExtName(Connection& conn, uint32_t assetId)
{
    static std::string sql = R"(
        SELECT
            asset.name AS name,
            ext.value  AS extName
        FROM
            t_bios_asset_element AS asset
        LEFT JOIN t_bios_asset_ext_attributes AS ext
            ON ext.id_asset_element = asset.id_asset_element
        WHERE
            ext.keytag = "name" AND asset.id_asset_element = :assetId
    )";

    try {
        auto res = conn.selectRow(sql, "assetId"_p = assetId);
        return Names{res.get<std::string>("name"), res.get<std::string>("extName")};
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(assetId));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetId));
    }
}

// =========================================================================================================================================
// Converts asset's extended name to its internal name
// =========================================================================================================================================

Expected<std::string> extNameToAssetName(const std::string& assetExtName)
{
    try {
        Connection db;
        return extNameToAssetName(db, assetExtName);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetExtName));
    }
}

Expected<std::string> extNameToAssetName(Connection& conn, const std::string& assetExtName)
{
    static const std::string sql = R"(
        SELECT
            a.name
        FROM
            t_bios_asset_element AS a
        INNER JOIN t_bios_asset_ext_attributes AS e
            ON a.id_asset_element = e.id_asset_element
        WHERE
            keytag = "name" and value = :extName
    )";

    try {
        auto res = conn.selectRow(sql, "extName"_p = assetExtName);
        return res.get("name");
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(assetExtName));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetExtName));
    }
}


// =========================================================================================================================================
// Converts internal name to extended name
// =========================================================================================================================================

Expected<std::string> nameToExtName(const std::string& assetName)
{
    try {
        Connection conn;
        return nameToExtName(conn, assetName);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetName));
    }
}

Expected<std::string> nameToExtName(Connection& conn, const std::string& assetName)
{
    static const std::string sql = R"(
        SELECT
            e.value
        FROM
            t_bios_asset_ext_attributes AS e
        INNER JOIN t_bios_asset_element AS a
            ON a.id_asset_element = e.id_asset_element
        WHERE
            keytag = 'name' AND a.name = :asset_name
    )";

    if (!persist::is_ok_name(assetName.c_str())) {
        return unexpected("'{}' name is not valid"_tr, assetName);
    }

    try {
        auto res = conn.selectRow(sql, "asset_name"_p = assetName);
        return res.get("value");
    } catch (const NotFound&) {
        return unexpected(error(fty::asset::Errors::ElementNotFound).format(assetName));
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), assetName));
    }
}

// =========================================================================================================================================
// Selects maximum number of power sources for device in the system
// =========================================================================================================================================

Expected<uint32_t> maxNumberOfPowerLinks()
{
    try {
        Connection conn;
        return maxNumberOfPowerLinks(conn);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

Expected<uint32_t> maxNumberOfPowerLinks(Connection& conn)
{
    static const std::string sql = R"(
        SELECT
            MAX(power_src_count) AS maxCount
        FROM
            (SELECT COUNT(*) power_src_count FROM t_bios_asset_link
                GROUP BY id_asset_device_dest) pwr
    )";

    try {
        auto res = conn.selectRow(sql);
        return res.get<uint32_t>("maxCount");
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

// =========================================================================================================================================
// Selects maximal number of groups in the system
// =========================================================================================================================================

Expected<uint32_t> maxNumberOfAssetGroups()
{
    try {
        Connection conn;
        return maxNumberOfAssetGroups(conn);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

Expected<uint32_t> maxNumberOfAssetGroups(Connection& conn)
{
    static const std::string sql = R"(
        SELECT
            MAX(grp_count) AS maxCount
        FROM
            ( SELECT COUNT(*) grp_count FROM t_bios_asset_group_relation
                GROUP BY id_asset_element) elmnt_grp
    )";

    try {
        auto res = conn.selectRow(sql);
        return res.get<uint32_t>("maxCount");
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::InternalError).format(e.what()));
    }
}

// =========================================================================================================================================
// Returns how many times is gived a couple keytag/value in t_bios_asset_ext_attributes
// =========================================================================================================================================

Expected<int32_t> countKeytag(const std::string& keytag, const std::string& value, uint32_t elementId)
{
    try {
        Connection conn;
        return countKeytag(conn, keytag, value, elementId);
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), keytag));
    }
}

Expected<int32_t> countKeytag(Connection& conn, const std::string& keytag, const std::string& value, uint32_t elementId)
{
    static const std::string sql = R"(
        SELECT
            COUNT(*) AS count
        FROM
            t_bios_asset_ext_attributes
        WHERE
            keytag = :keytag AND
            value = :value
    )";

    static const std::string sqlEl = R"(
        SELECT
            id_asset_element,
            COUNT(id_asset_element) AS count
        FROM
            t_bios_asset_ext_attributes
        WHERE
            keytag = :keytag AND
            value = :value AND
            id_asset_element = :element_id
    )";

    try {
        auto st = conn.prepare(elementId ? sqlEl : sql);

        // clang-format off
        st.bind(
            "keytag"_p = keytag,
            "value"_p  = value
        );
        // clang-format on

        if (elementId) {
            st.bind("element_id"_p = elementId);
        }

        return st.selectRow().get<int32_t>("count");
    } catch (const NotFound&) {
        if (elementId) {
            return unexpected(error(fty::asset::Errors::ElementNotFound).format(elementId));
        } else {
            return unexpected(error(fty::asset::Errors::ElementNotFound).format(keytag));
        }
    } catch (const std::exception& e) {
        return unexpected(error(fty::asset::Errors::ExceptionForElement).format(e.what(), keytag));
    }
}

} // namespace fty::db::asset
