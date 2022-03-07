/*  =========================================================================
    dbhelpers - Helpers functions for database

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    dbhelpers - Helpers functions for database
@discuss
@end
*/


#include "asset/dbhelpers.h"

#include "fty_proto.h"
#include "fty_asset_dto.h"
#include "fty_asset_server.h"
#include <fty_log.h>
#include <cxxtools/jsonserializer.h>

#define INPUT_POWER_CHAIN     1
#define AGENT_ASSET_ACTIVATOR "etn-licensing-credits"

// for test purposes
std::map<std::string, std::string> test_map_asset_state;

/**
 *  \brief Wrapper for select_assets_by_container_cb
 *
 *  \param[in] container_name - iname of container
 *  \param[in] filter - types and subtypes we are interested in
 *  \param[out] assets - inames of assets in this container
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int select_assets_by_container(const std::string& container_name, const std::set<std::string>& filter,
    std::vector<std::string>& assets, bool test)
{
    if (test)
        return 0;
    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    [[maybe_unused]] int rv = DBAssets::select_assets_by_container_name_filter(conn, container_name, filter, assets);
    return rv;
}

/**
 *  \brief Selects basic asset info
 *
 *  \param[in] element_id - iname of asset
 *  \param[in] cb - function to call on each row
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int select_asset_element_basic(
    const std::string& asset_name, std::function<void(const tntdb::Row&)> cb, bool test)
{
    if (test)
        return 0;
    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    int               rv   = DBAssets::select_asset_element_basic_cb(conn, asset_name, cb);
    return rv;
}

/**
 *  \brief Selects ext attributes for given asset
 *
 *  \param[in] element_id - iname of asset
 *  \param[in] cb - function to call on each row
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int select_ext_attributes(uint32_t asset_id, std::function<void(const tntdb::Row&)> cb, bool test)
{
    if (test)
        return 0;
    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    int               rv   = DBAssets::select_ext_attributes_cb(conn, asset_id, cb);
    return rv;
}

/**
 *  \brief Selects all parents of given asset
 *
 *  \param[in] id - iname of asset
 *  \param[in] cb - function to call on each row
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int select_asset_element_super_parent(uint32_t id, std::function<void(const tntdb::Row&)>& cb, bool test)
{
    if (test)
        return 0;
    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    int               rv   = DBAssets::select_asset_element_super_parent(conn, id, cb);
    return rv;
}

/**
 * Wrapper for select_assets_by_filter_cb
 *
 * \param[in] filter - types/subtypes we are interested in
 * \param[out] assets - inames of returned assets
 */
int select_assets_by_filter(const std::set<std::string>& filter, std::vector<std::string>& assets, bool test)
{
    if (test)
        return 0;

    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    int               rv   = DBAssets::select_assets_by_filter(conn, filter, assets);
    return rv;
}

/**
 *  \brief Selects basic asset info for all assets in the DB
 *
 *  \param[in] cb - function to call on each row
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int select_assets(std::function<void(const tntdb::Row&)>& cb, bool test)
{
    if (test)
        return 0;
    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    int               rv   = DBAssets::select_assets_cb(conn, cb);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

#define SQL_EXT_ATT_INVENTORY                                                                                \
    " INSERT INTO"                                                                                           \
    "   t_bios_asset_ext_attributes"                                                                         \
    "   (keytag, value, id_asset_element, read_only)"                                                        \
    " VALUES"                                                                                                \
    "  ( :keytag, :value, (SELECT id_asset_element FROM t_bios_asset_element WHERE name=:device_name), "     \
    ":readonly)"                                                                                             \
    " ON DUPLICATE KEY"                                                                                      \
    "   UPDATE "                                                                                             \
    "       value = VALUES (value),"                                                                         \
    "       read_only = :readonly,"                                                                          \
    "       id_asset_ext_attribute = LAST_INSERT_ID(id_asset_ext_attribute)"
/**
 *  \brief Inserts ext attributes from inventory message into DB
 *
 *  \param[in] device_name - iname of assets
 *  \param[in] ext_attributes - recent ext attributes for this asset
 *  \param[in] read_only - whether to insert ext attributes as readonly
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int process_insert_inventory(
    const std::string& device_name, zhash_t* ext_attributes, bool readonly, bool test)
{
    if (test)
        return 0;
    if (!ext_attributes || (zhash_size(ext_attributes) == 0))
        return 0;

    tntdb::Connection conn;
    try {
        conn = tntdb::connectCached(DBConn::url);
    } catch (const std::exception& e) {
        log_error("DB: cannot connect, %s", e.what());
        return -1;
    }

    tntdb::Transaction trans(conn);
    tntdb::Statement   st = conn.prepareCached(SQL_EXT_ATT_INVENTORY);

    for (void* it = zhash_first(ext_attributes); it != NULL; it = zhash_next(ext_attributes)) {

        const char* value     = static_cast<const char*>(it);
        const char* keytag    = zhash_cursor(ext_attributes);
        bool        readonlyV = readonly;

        if (strcmp(keytag, "name") == 0 || strcmp(keytag, "description") == 0 ||
            strcmp(keytag, "ip.1") == 0) {
            readonlyV = false;
        }
        if (strcmp(keytag, "uuid") == 0) {
            readonlyV = true;
        }
        try {
            st.set("keytag", keytag)
                .set("value", value)
                .set("device_name", device_name)
                .set("readonly", readonlyV)
                .execute();
        } catch (const std::exception& e) {
            log_warning("%s:\texception on updating %s {%s, %s}\n\t%s", "", device_name.c_str(), keytag,
                value, e.what());
            continue;
        }
    }

    trans.commit();
    return 0;
}

/**
 *  \brief Inserts ext attributes from inventory message into DB only
 *         if the new value is different from the cache map
 *         This method is not thread safe.
 *
 *  \param[in] device_name - iname of assets
 *  \param[in] ext_attributes - recent ext attributes for this asset
 *  \param[in] read_only - whether to insert ext attributes as readonly
 *  \param[in] test - unit tests indicator
 *  \param[in] map_cache -  cache map
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int process_insert_inventory(const std::string& device_name, zhash_t* ext_attributes, bool readonly,
    std::unordered_map<std::string, std::string>& map_cache, bool test)
{
    if (test)
        return 0;
    if (!ext_attributes || (zhash_size(ext_attributes) == 0))
        return 0;

    tntdb::Connection conn;
    try {
        conn = tntdb::connectCached(DBConn::url);
    } catch (const std::exception& e) {
        log_error("DB: cannot connect, %s", e.what());
        return -1;
    }

    tntdb::Transaction trans(conn);
    tntdb::Statement   st = conn.prepareCached(SQL_EXT_ATT_INVENTORY);

    for (void* it = zhash_first(ext_attributes); it != NULL; it = zhash_next(ext_attributes)) {
        const char* value     = static_cast<const char*>(it);
        const char* keytag    = zhash_cursor(ext_attributes);
        bool        readonlyV = readonly;
        if (strcmp(keytag, "name") == 0 || strcmp(keytag, "description") == 0)
            readonlyV = false;

        std::string cache_key = device_name;
        cache_key.append(":").append(keytag).append(readonlyV ? "1" : "0");
        auto el = map_cache.find(cache_key);
        if (el != map_cache.end() && el->second == value)
            continue;
        try {
            st.set("keytag", keytag)
                .set("value", value)
                .set("device_name", device_name)
                .set("readonly", readonlyV)
                .execute();
            map_cache[cache_key] = value;
        } catch (const std::exception& e) {
            log_warning("%s:\texception on updating %s {%s, %s}\n\t%s", "", device_name.c_str(), keytag,
                value, e.what());
            continue;
        }
    }

    trans.commit();
    return 0;
}
/**
 *  \brief Selects user-friendly name for given asset name
 *
 *  \param[in] iname - asset iname
 *  \param[out] ename - user-friendly asset name
 *  \param[in] test - unit tests indicator
 *
 *  \return  0 - in case of success
 *          -1 - in case of some unexpected error
 */
int select_ename_from_iname(std::string& iname, std::string& ename, bool test)
{
    if (test) {
        if (iname == TEST_INAME) {
            ename = TEST_ENAME;
            return 0;
        } else
            return -1;
    }
    try {

        tntdb::Connection conn = tntdb::connectCached(DBConn::url);
        tntdb::Statement  st   = conn.prepareCached(
            "SELECT e.value FROM  t_bios_asset_ext_attributes AS e "
            "INNER JOIN t_bios_asset_element AS a "
            "ON a.id_asset_element = e.id_asset_element "
            "WHERE keytag = 'name' and a.name = :iname; "

        );

        tntdb::Row row = st.set("iname", iname).selectRow();
        log_debug("[s_handle_subject_ename_from_iname]: were selected %" PRIu32 " rows", 1);

        row[0].get(ename);
    } catch (const std::exception& e) {
        log_error("exception caught %s for element '%s'", e.what(), ename.c_str());
        return -1;
    }

    return 0;
}


/**
 *  \brief Get number of active power devices
 *
 *  \return  X - number of active power devices
 */

int get_active_power_devices(bool test)
{
    int count = 0;
    if (test) {
        log_debug("[get_active_power_devices]: runs in test mode");
        for (auto const& as : test_map_asset_state) {
            if ("active" == as.second) {
                ++count;
            }
        }
        return count;
    }
    tntdb::Connection conn = tntdb::connectCached(DBConn::url);
    return DBAssets::get_active_power_devices(conn);
}

bool should_activate(std::string operation, std::string current_status, std::string new_status)
{
    bool rv = (((operation == FTY_PROTO_ASSET_OP_CREATE) || (operation == "create-force")) &&
               (new_status == "active"));
    rv |= (operation == FTY_PROTO_ASSET_OP_UPDATE && current_status == "nonactive" && new_status == "active");

    return rv;
}

bool should_deactivate(std::string operation, std::string current_status, std::string new_status)
{
    bool rv =
        (operation == FTY_PROTO_ASSET_OP_UPDATE && current_status == "active" && new_status == "nonactive");
    rv |= (operation == FTY_PROTO_ASSET_OP_DELETE);

    return rv;
}
