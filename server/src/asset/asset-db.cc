/*  =========================================================================
    asset_db - asset_db

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

#include "asset-db.h"
#include "asset.h"
#include "fty-lock.h"
#include "asset/asset-helpers.h"

#include <fty_common_db_dbpath.h>
#include <cstdlib>
#include <sstream>
#include <tntdb.h>
#include <map>
#include <algorithm>

#include <cassert>

namespace fty {

// static helpers

// DB
DB::DB()
{
    // m_conn = tntdb::connectCached(DBConn::url);
}

DB& DB::getInstance()
{
    static DB m_instance;

    // fetches connection from pool, if exists
    // creates a new connection otherwise
    m_instance.m_conn = tntdb::connectCached(DBConn::url);

    return m_instance;
}

void DB::loadAsset(const std::string& nameId, Asset& asset)
{
    tntdb::Row row;

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            a.id_asset_element AS id,
            a.name             AS name,
            e.name             AS type,
            d.name             AS subType,
            p.name             AS parentName,
            a.status           AS status,
            a.priority         AS priority,
            a.asset_tag        AS tag,
            a.id_secondary     AS idSecondary
        FROM t_bios_asset_element AS a
            INNER JOIN t_bios_asset_device_type AS d
            INNER JOIN t_bios_asset_element_type AS e
            ON a.id_type = e.id_asset_element_type AND a.id_subtype = d.id_asset_device_type
            LEFT JOIN t_bios_asset_element AS p
            ON a.id_parent = p.id_asset_element
        WHERE a.name = :asset_name
    )");
    q.set("asset_name", nameId);
    // clang-format on

    try {
        Lock lock(m_conn_lock);
        row = q.selectRow();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    asset.setInternalName(row.getString("name"));
    asset.setAssetType(row.getString("type"));
    asset.setAssetSubtype(row.getString("subType"));
    if (!row.isNull("parentName")) {
        asset.setParentIname(row.getString("parentName"));
    }
    asset.setAssetStatus(stringToAssetStatus(row.getString("status")));
    asset.setPriority(row.getInt("priority"));
    if (!row.isNull("tag")) {
        asset.setAssetTag(row.getString("tag"));
    }
    if (!row.isNull("idSecondary")) {
        asset.setSecondaryID(row.getString("idSecondary"));
    }
}

void DB::loadExtMap(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            keytag,
            value,
            read_only
        FROM
            t_bios_asset_ext_attributes
        WHERE
            id_asset_element = :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    asset.clearExtMap();

    for (const auto& row : res) {
        asset.setExtEntry(row.getString("keytag"), row.getString("value"), row.getBool("read_only"), true);
    }
}

std::vector<std::string> DB::getChildren(const Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            name
        FROM
            t_bios_asset_element
        WHERE
            id_parent = :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    std::vector<std::string> children;
    for (const auto& row : res) {
        children.push_back(row.getString("name"));
    }

    return children;
}

// returns fty::unexpected if internal name is not found, the integer ID otherwise
fty::Expected<uint32_t> DB::getID(const std::string& internalName)
{
    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            id_asset_element
        FROM
            t_bios_asset_element
        WHERE
            name = :internal_name
    )");
    // clang-format on
    q.set("internal_name", internalName);

    uint32_t assetID = 0;

    try {
        Lock lock(m_conn_lock);
        auto v = q.selectValue();


        assetID = static_cast<uint32_t>(v.getInt32());
    } catch (tntdb::NotFound&) {

        return fty::unexpected("Internal name {} not found", internalName);
    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return assetID;
}

uint32_t DB::getTypeID(const std::string& type)
{
    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            id_asset_element_type
        FROM
            t_bios_asset_element_type
        WHERE
            name = :type_name
    )");
    // clang-format on
    q.set("type_name", type);

    uint32_t typeID = 0;

    try {
        Lock lock(m_conn_lock);
        auto v = q.selectValue();


        typeID = static_cast<uint32_t>(v.getInt32());
    } catch (tntdb::NotFound&) {

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return typeID;
}
uint32_t DB::getSubtypeID(const std::string& subtype)
{
    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            id_asset_device_type
        FROM
            t_bios_asset_device_type
        WHERE
            name = :subtype_name
    )");
    // clang-format on
    q.set("subtype_name", subtype);

    uint32_t subtypeID = 0;

    try {
        Lock lock(m_conn_lock);
        auto v = q.selectValue();


        subtypeID = static_cast<uint32_t>(v.getInt32());
    } catch (tntdb::NotFound&) {

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return subtypeID;
}

bool DB::verifyID(std::string& id) {
    // clang-format off
    auto q = m_conn.prepare(R"(
        SELECT
            COUNT(id_asset_element)
        FROM
            t_bios_asset_element
        WHERE
            name like :name
    )");
    // clang-format on
    q.set("name", "%" + id);

    int res;
    try {
        Lock lock(m_conn_lock);
        res = q.selectValue().getInt();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return res == 0;
}

uint32_t DB::getLinkID(const uint32_t destId, const AssetLink& l)
{
    uint32_t linkID = 0;

    auto srcId = getID(l.sourceId());
    if(!srcId) {
        throw std::runtime_error(srcId.error());
    }

    assert(destId);

    tntdb::Statement q;

    std::stringstream qs;

    // clang-format off
    qs <<
        " SELECT "                          \
        "    id_link as id "                \
        " FROM "                            \
        "    t_bios_asset_link"             \
        " WHERE"                            \
        "    id_asset_device_src = :src"    \
        "    AND"                           \
        "    id_asset_device_dest = :dest";
    if(!l.srcOut().empty()) {
        qs <<
        "    AND"                   \
        "    src_out = :srcOut";
    } else {
        qs <<
        "    AND"                   \
        "    src_out is NULL";
    }
    if(!l.destIn().empty()) {
        qs <<
        "    AND"                   \
        "    dest_in = :destIn";
    } else {
        qs <<
        "    AND"                   \
        "    dest_in is NULL";
    }
    qs <<
        "    AND" \
        "    id_asset_link_type = :linkType";
    // clang-format off

    q = m_conn.prepareCached(qs.str().c_str());

    q.set("src", *srcId);
    q.set("dest", destId);

    if (!l.srcOut().empty())
        q.set("srcOut", l.srcOut());
    if (!l.destIn().empty())
        q.set("destIn", l.destIn());

    q.set("linkType", l.linkType());

    try {
        Lock lock(m_conn_lock);
        auto v = q.selectValue();


        linkID = v.getUnsigned32();
    } catch (tntdb::NotFound&) {

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return linkID;
}

void DB::loadLinkExtMap(const uint32_t linkID, AssetLink& link)
{
    assert(linkID);

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            keytag,
            value,
            read_only
        FROM
            t_bios_asset_link_attributes
        WHERE
            id_link = :link_id
    )");
    // clang-format on
    q.set("link_id", linkID);

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    link.clearExtMap();

    for (const auto& row : res) {
        link.setExtEntry(row.getString("keytag"), row.getString("value"), row.getBool("read_only"), true);
    }
}

void DB::saveLinkExtMap(const uint32_t linkID, const AssetLink& link)
{
    /*
     * Here is the strategy to save the external attributes:
     * 1. We insert, update or remove only the external attribute which has been modified.
     * 2. An external attribute with a value set to empty string will be removed from the db
     */

    if (linkID == 0) {
        throw std::runtime_error("Link with ID = " + std::to_string(linkID) + " not found");
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            id_asset_link_attribute AS id,
            keytag                  AS keytag,
            value                   AS val,
            read_only               AS read_only
        FROM
            t_bios_asset_link_attributes
        WHERE
            id_link = :link_id
    )");
    // clang-format on
    q.set("link_id", linkID);

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    using ExternalAttributeInDB = std::tuple<uint32_t, std::string, std::string, bool>;

    std::vector<ExternalAttributeInDB> existing;
    for (const auto& row : res) {
        existing.emplace_back(
            row.getUnsigned32("id"), row.getString("keytag"), row.getString("val"), row.getBool("read_only"));
    }

    std::vector<ExternalAttributeInDB> toBeRemoved;


    for (const auto& it : link.ext()) {

        // skip the none updated attribute
        if (!it.second.wasUpdated()) {
            continue;
        }

        auto found = std::find_if(existing.begin(), existing.end(), [&](const ExternalAttributeInDB& e) {
            return std::get<1>(e) == it.first;
        });


        if (found == existing.end()) {
            // The attribute do not exist in the database
            // if it's not empty we insert it.

            if (!it.second.getValue().empty()) {
                // clang-format off
                auto q_ext_link = m_conn.prepareCached(R"(
                    INSERT INTO t_bios_asset_link_attributes (keytag, value, id_link, read_only)
                    VALUES (:key, :value, :linkId, :readOnly)
                )");
                // clang-format on
                q_ext_link.set("key", it.first);
                q_ext_link.set("value", it.second.getValue());
                q_ext_link.set("readOnly", it.second.isReadOnly());
                q_ext_link.set("linkId", linkID);
                try {
                    Lock lock(m_conn_lock);
                    q_ext_link.execute();

                } catch (std::exception& e) {

                    throw std::runtime_error("database error - " + std::string(e.what()));
                }
            }

        } else {
            // The attribute exist in the database
            // if it's not empty we update it, else remove it.

            if (!it.second.getValue().empty()) {
                // clang-format off
                auto q_ext_link = m_conn.prepareCached(R"(
                    UPDATE t_bios_asset_link_attributes
                    SET
                        value = :value,
                        read_only = :readOnly
                    WHERE id_asset_link_attribute = :extId
                )");
                // clang-format on
                q_ext_link.set("value", it.second.getValue());
                q_ext_link.set("readOnly", it.second.isReadOnly());
                q_ext_link.set("extId", std::get<0>(*found));

                try {
                    Lock lock(m_conn_lock);
                    q_ext_link.execute();

                } catch (std::exception& e) {

                    throw std::runtime_error("database error - " + std::string(e.what()));
                }
            } else {
                toBeRemoved.push_back(*found);
            }
        }
    }

    for (const auto& toRem : toBeRemoved) {
        // clang-format off
        auto q_ext_link = m_conn.prepareCached(R"(
            DELETE FROM t_bios_asset_link_attributes
            WHERE id_asset_link_attribute = :extId
        )");
        // clang-format on
        q_ext_link.set("extId", std::get<0>(toRem));

        try {
            Lock lock(m_conn_lock);
            q_ext_link.execute();

        } catch (std::exception& e) {

            throw std::runtime_error("database error - " + std::string(e.what()));
        }
    }
}

void DB::loadLinkedAssets(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    };

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            l.id_link               AS link_id,
            e.name                  AS name,
            l.src_out               AS srcOut,
            l.dest_in               AS destIn,
            l.id_asset_link_type    AS linkType
        FROM
            t_bios_asset_link AS l
        INNER JOIN
            t_bios_asset_element AS e ON l.id_asset_device_src = e.id_asset_element
        WHERE
            l.id_asset_device_dest = :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    std::vector<AssetLink> links;
    for (const auto& row : res) {
        uint32_t    linkID = row.getUnsigned32("link_id");
        std::string srcOut, destIn;
        // may be NULL
        if (!row.isNull("srcOut")) {
            row.getString("srcOut", srcOut);
        }
        if (!row.isNull("destIn")) {
            row.getString("destIn", destIn);
        }

        AssetLink l(row.getString("name"), srcOut, destIn, row.getInt("linkType"));
        loadLinkExtMap(linkID, l);

        links.push_back(l);
    }
    asset.setLinkedAssets(links);
}

void DB::saveLinkedAssets(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());

    if (!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            e.id_asset_element   AS srcId,
            e.name               AS srcName,
            l.src_out            AS srcOut,
            l.dest_in            AS destIn,
            l.id_asset_link_type AS linkType
        FROM t_bios_asset_link   AS l
        INNER JOIN
            t_bios_asset_element AS e
            ON e.id_asset_element = l.id_asset_device_src
        WHERE
             id_asset_device_dest = :assetId
    )");
    // clang-format on
    q.set("assetId", *assetID);

    tntdb::Result res;
    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    using Existing = std::pair<uint32_t, AssetLink>;

    // get existings links from database
    std::vector<Existing> toRemove;
    for (const auto& row : res) {
        std::string tmpOut, tmpIn;

        // may be NULL
        if (!row.isNull("srcOut")) {
            row.getString("srcOut", tmpOut);
        }
        if (!row.isNull("destIn")) {
            row.getString("destIn", tmpIn);
        }

        toRemove.emplace_back(row.getUnsigned32("srcId"),
            AssetLink(row.getString("srcName"), tmpOut, tmpIn, row.getInt("linkType")));
    }

    // add new links
    for (const AssetLink& l : asset.getLinkedAssets()) {
        // delete link from the list of links to remove
        auto found = std::find_if(toRemove.begin(), toRemove.end(), [&](const Existing& e) {
            return e.second == l;
        });

        if (found != toRemove.end()) {
            // link is required, do not remove
            toRemove.erase(found);
        } else {
            // save new link
            saveLink(*assetID, l);
        }
    }

    // remove links not present in DTO
    for (const auto& entry : toRemove) {
        const AssetLink& l = entry.second;

        // remove all remaining links
        removeLink(*assetID, l);
    }
}

bool DB::hasLinkedAssets(const Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepare(R"(
        SELECT
            COUNT(id_link)
        FROM
            t_bios_asset_link
        WHERE
            id_asset_device_src = :src
    )");
    // clang-format on
    q.set("src", *assetID);

    int linkedAssets;
    try {
        Lock lock(m_conn_lock);
        linkedAssets = q.selectValue().getInt();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return linkedAssets != 0;
}

void DB::saveLink(const uint32_t destId, const AssetLink& l)
{
    auto srcId = getID(l.sourceId());
    if(!srcId) {
        throw std::runtime_error(srcId.error());
    }

    assert(destId);

    uint32_t linkId = 0;

    // clang-format off
    auto q1 = m_conn.prepareCached(R"(
        INSERT INTO
            t_bios_asset_link
            (id_asset_device_src, src_out, id_asset_device_dest, dest_in, id_asset_link_type)
        VALUES (
            :src,
            :srcOut,
            :dest,
            :destIn,
            :linkType
        )
    )");
    // clang-format on

    q1.set("src", *srcId);
    q1.set("dest", destId);

    l.srcOut().empty() ? q1.setNull("srcOut") : q1.set("srcOut", l.srcOut());
    l.destIn().empty() ? q1.setNull("destIn") : q1.set("destIn", l.destIn());

    q1.set("linkType", l.linkType());

    try {
        Lock lock(m_conn_lock);
        q1.execute();
        linkId = static_cast<uint32_t>(m_conn.lastInsertId());

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    if (!l.ext().empty()) {
        saveLinkExtMap(linkId, l);
    }
}

void DB::removeLink(const uint32_t destId, const AssetLink& l)
{
    assert(destId);

    uint32_t linkId = getLinkID(destId, l);

    if (linkId) {
        // clang-format off
        auto q_ext_attrib = m_conn.prepareCached(R"(
            DELETE FROM
                t_bios_asset_link_attributes
            WHERE
                id_link = :link_id
        )");
        // clang-format on

        q_ext_attrib.set("link_id", linkId);

        try {
            Lock lock(m_conn_lock);
            q_ext_attrib.execute();

        } catch (std::exception& e) {

            throw std::runtime_error("database error - " + std::string(e.what()));
        }

        // clang-format off
        auto q_link = m_conn.prepareCached(R"(
            DELETE FROM
                t_bios_asset_link
            WHERE
                id_link = :link_id
        )");
        // clang-format on

        q_link.set("link_id", linkId);

        try {
            Lock lock(m_conn_lock);
            q_link.execute();

        } catch (std::exception& e) {

            throw std::runtime_error("database error - " + std::string(e.what()));
        }
    }
}

void DB::unlinkAll(Asset& dest)
{
    auto destID = getID(dest.getInternalName());
    if(!destID) {
        throw std::runtime_error(destID.error());
    }

    for (const auto& link : dest.getLinkedAssets()) {
        removeLink(*destID, link);
    }
}

bool DB::isLastDataCenter(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepare(R"(
        SELECT
            COUNT(id_asset_element)
        FROM
            t_bios_asset_element
        WHERE
            id_type = (
                SELECT id_asset_element_type
                FROM   t_bios_asset_element_type
                WHERE  name = 'datacenter'
            )
            AND id_asset_element != :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    int numDatacentersAfterDelete = -1;

    try {
        Lock lock(m_conn_lock);
        numDatacentersAfterDelete = q.selectValue().getInt();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return numDatacentersAfterDelete == 0;
}

void DB::removeFromGroups(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        DELETE FROM
            t_bios_asset_group_relation
        WHERE
            id_asset_element = :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    try {
        Lock lock(m_conn_lock);
        q.execute();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }
}

void DB::removeFromRelations(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        DELETE FROM
            t_bios_monitor_asset_relation
        WHERE
            id_asset_element = :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    try {
        Lock lock(m_conn_lock);
        q.execute();

    } catch (std::exception& e) {

        throw std::runtime_error(std::string(e.what()));
    }
}

void DB::removeAsset(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        DELETE FROM
            t_bios_asset_element
        WHERE
            id_asset_element = :asset_id
    )");
    // clang-format on
    q.set("asset_id", *assetID);

    try {
        Lock lock(m_conn_lock);
        q.execute();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }
}

void DB::removeExtMap(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        DELETE FROM
            t_bios_asset_ext_attributes
        WHERE
            id_asset_element = :assetId
    )");
    // clang-format on
    q.set("assetId", *assetID);

    try {
        Lock lock(m_conn_lock);
        q.execute();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }
}

void DB::clearGroup(Asset& asset)
{
    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        DELETE FROM
            t_bios_asset_group_relation
        WHERE
            d_asset_group = :grp
    )");
    // clang-format on
    q.set("grp", *assetID);

    try {
        Lock lock(m_conn_lock);
        q.execute();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }
}

void DB::beginTransaction()
{
    Lock lock(m_conn_lock);
    m_conn.beginTransaction();

}

void DB::rollbackTransaction()
{
    Lock lock(m_conn_lock);
    m_conn.rollbackTransaction();

}

void DB::commitTransaction()
{
    Lock lock(m_conn_lock);
    m_conn.commitTransaction();

}

void DB::update(Asset& asset)
{
    if (asset.getInternalName().empty()) {
        log_error("Asset iname is empty");
        throw std::runtime_error("Asset iname is empty");
    }
    if (asset.getInternalName() == asset.getParentIname()) {
        log_error("Asset iname is same as parent iname (iname: %s)", asset.getInternalName().c_str());
        throw std::runtime_error("Asset iname is same as parent iname");
    }

    // get asset id
    uint32_t assetId = 0;
    {
        auto val = getID(asset.getInternalName());
        if (!val) {
            log_error("Error getting id of %s (%s)", asset.getInternalName().c_str(), val.error().c_str());
            throw std::runtime_error(val.error());
        }
        assetId = *val;
    }

    if (assetId == 0) {
        log_error("Unexpected behaviour on asset id (%s, assetId: %u)", asset.getInternalName().c_str(), assetId);
        throw std::runtime_error("Unexpected behaviour on asset id");
    }

    // if parent name is not empty, check if it exists
    uint32_t parentId = 0;
    const std::string& parentIname = asset.getParentIname();
    if (!parentIname.empty()) {
        auto val = getID(parentIname);
        if (!val) {
            log_error("Error getting id of %s (%s)", parentIname.c_str(), val.error().c_str());
            throw std::runtime_error(val.error());
        }
        parentId = *val;
    }

    if (assetId == parentId) {
        log_error("Unexpected behaviour on parent id of %s (id: %u): parent '%s' (parentId: %u)",
            asset.getInternalName().c_str(), assetId, parentIname.c_str(), parentId);
        throw std::runtime_error("Unexpected behaviour on parent id");
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        UPDATE
            t_bios_asset_element
        SET
            id_parent = :parent_id,
            status = :status,
            priority = :priority,
            asset_tag = :assetTag,
            id_secondary = :idSecondary
        WHERE
            id_asset_element = :assetId
    )");
    // clang-format on

    q.set("assetId", assetId);

    // name field can't be null, parent id is set to NULL if parentIname is empty
    if (parentId == 0) {
        log_debug("Update asset '%s' (id: %u) with NULL parent", asset.getInternalName().c_str(), assetId);
        q.setNull("parent_id");
    }
    else {
        log_debug("Update asset '%s' (id: %u) with parent '%s' (parentId: %u)",
            asset.getInternalName().c_str(), assetId, parentIname.c_str(), parentId);
        q.set("parent_id", parentId);
    }

    q.set("status", assetStatusToString(asset.getAssetStatus()));
    q.set("priority", asset.getPriority());

    if (asset.getAssetTag().empty()) q.setNull("assetTag");
    else q.set("assetTag", asset.getAssetTag());

    if (asset.getSecondaryID().empty()) q.setNull("idSecondary");
    else q.set("idSecondary", asset.getSecondaryID());

    try {
        Lock lock(m_conn_lock);
        q.execute();
    }
    catch (std::exception& e) {
        throw std::runtime_error("database error - " + std::string(e.what()));
    }
}

void DB::insert(Asset& asset)
{
    if (asset.getInternalName().empty()) {
        log_error("Asset iname is empty");
        throw std::runtime_error("Asset iname is empty");
    }
    if (asset.getInternalName() == asset.getParentIname()) {
        log_error("Asset iname is same as parent iname (iname: %s)", asset.getInternalName().c_str());
        throw std::runtime_error("Asset iname is same as parent iname");
    }

    // if parent name is not empty, check if it exists
    uint32_t parentId = 0;
    const std::string& parentIname = asset.getParentIname();
    if (!parentIname.empty()) {
        auto val = getID(parentIname);
        if (!val) {
            log_error("Error getting id of %s (%s)", parentIname.c_str(), val.error().c_str());
            throw std::runtime_error(val.error());
        }
        parentId = *val;
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        INSERT INTO
            t_bios_asset_element
            (name, id_type, id_subtype, id_parent, status, priority, asset_tag, id_secondary)
        VALUES (
            :name,
            (SELECT id_asset_element_type FROM t_bios_asset_element_type WHERE name = :type),
            (SELECT id_asset_device_type FROM t_bios_asset_device_type WHERE name = :subtype),
            :parent_id,
            :status,
            :priority,
            :asset_tag,
            :idSecondary
        )
    )");
    // clang-format on

    q.set("name", asset.getInternalName());
    q.set("type", asset.getAssetType());
    q.set("subtype", asset.getAssetSubtype());

    // name field can't be null, parent id is set to NULL if parentIname is empty
    if (parentId == 0) {
        log_debug("Insert asset '%s' with NULL parent", asset.getInternalName().c_str());
        q.setNull("parent_id");
    }
    else {
        log_debug("Insert asset '%s' with parent '%s' (parentId: %u)",
            asset.getInternalName().c_str(), parentIname.c_str(), parentId);
        q.set("parent_id", parentId);
    }

    // always insert as non active, update after activation
    q.set("status", assetStatusToString(fty::AssetStatus::Nonactive));
    q.set("priority", asset.getPriority());

    if (asset.getAssetTag().empty()) q.setNull("assetTag");
    else q.set("assetTag", asset.getAssetTag());

    if (asset.getSecondaryID().empty()) q.setNull("idSecondary");
    else q.set("idSecondary", asset.getSecondaryID());

    try {
        Lock lock(m_conn_lock);
        q.execute();
    }
    catch (std::exception& e) {
        throw std::runtime_error("database error - " + std::string(e.what()));
    }
}

std::string DB::inameById(uint32_t id)
{
    std::string res;

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT name FROM t_bios_asset_element WHERE id_asset_element = :assetId
    )");
    // clang-format on
    q.set("assetId", id);

    try {
        Lock lock(m_conn_lock);
        res = q.selectRow().getString("name");

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return res;
}

std::string DB::inameByUuid(const std::string& uuid)
{
    std::string res;
    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            name
        FROM
            t_bios_asset_element
        WHERE
            id_asset_element = (
                SELECT
                    id_asset_element
                FROM
                    t_bios_asset_ext_attributes
                WHERE
                    keytag = "uuid" AND value = :uuid
            )
        )");
    q.set("uuid", uuid);
    // clang-format on

    try {
        Lock lock(m_conn_lock);
        res = q.selectRow().getString("name");

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    return res;
}

void DB::saveExtMap(Asset& asset)
{
    /*
     * Here is the strategy to save the external attributes:
     * 1. We insert, update or remove only the external attribute which has been modified.
     * 2. An external attribute with a value set to empty string will be removed from the db
     */

    auto assetID = getID(asset.getInternalName());
    if(!assetID) {
        throw std::runtime_error(assetID.error());
    }

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            id_asset_ext_attribute AS id,
            keytag                 AS akey,
            value                  AS avalue,
            read_only              AS readOnly
        FROM t_bios_asset_ext_attributes
        WHERE
             id_asset_element = : assetId
    )");
    // clang-format on
    q.set("assetId", *assetID);

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    using ExternalAttributeInDB = std::tuple<uint32_t, std::string, std::string, bool>;

    std::vector<ExternalAttributeInDB> existing;
    for (const auto& row : res) {
        existing.emplace_back(
            row.getUnsigned32("id"), row.getString("akey"), row.getString("avalue"), row.getBool("readOnly"));
    }

    std::vector<ExternalAttributeInDB> toBeRemoved;


    for (const auto& it : asset.getExt()) {

        // skip the none updated attribute
        if (!it.second.wasUpdated()) {
            continue;
        }

        std::string value = it.second.getValue();
        if (it.first == "name" && it.second.getValue().size() > 50) {
            if (auto norm = fty::asset::normName(value, 50, *assetID)) {
                value = *norm;
            } else {
                logError("Cannot norm name: {}", norm.error());
                throw std::runtime_error("Cannot norm name - " + norm.error());
            }
        }

        auto found = std::find_if(existing.begin(), existing.end(), [&](const ExternalAttributeInDB& e) {
            return std::get<1>(e) == it.first;
        });


        if (found == existing.end()) {
            // The attribute do not exist in the database
            // if it's not empty we insert it.

            if (!it.second.getValue().empty()) {
                // clang-format off
                auto q1 = m_conn.prepareCached(R"(
                    INSERT INTO t_bios_asset_ext_attributes (keytag, value, id_asset_element, read_only)
                    VALUES (:key, :value, :assetId, :readOnly)
                )");
                // clang-format on
                q1.set("key", it.first);
                q1.set("value", value);
                q1.set("readOnly", it.second.isReadOnly());
                q1.set("assetId", *assetID);
                try {
                    Lock lock(m_conn_lock);
                    q1.execute();

                } catch (std::exception& e) {

                    throw std::runtime_error("database error - " + std::string(e.what()));
                }
            }

        } else {
            // The attribute exist in the database
            // if it's not empty we update it, else remove it.

            if (!it.second.getValue().empty()) {
                // clang-format off
                auto q1 = m_conn.prepareCached(R"(
                    UPDATE t_bios_asset_ext_attributes
                    SET
                        value = :value,
                        read_only = :readOnly
                    WHERE id_asset_ext_attribute = :extId
                )");
                // clang-format on
                q1.set("value", value);
                q1.set("readOnly", it.second.isReadOnly());
                q1.set("extId", std::get<0>(*found));

                try {
                    Lock lock(m_conn_lock);
                    q1.execute();

                } catch (std::exception& e) {

                    throw std::runtime_error("database error - " + std::string(e.what()));
                }
            } else {
                toBeRemoved.push_back(*found);
            }
        }
    }

    for (const auto& toRem : toBeRemoved) {
        // clang-format off
        auto q1 = m_conn.prepareCached(R"(
            DELETE FROM t_bios_asset_ext_attributes
            WHERE id_asset_ext_attribute = :extId
        )");
        // clang-format on
        q1.set("extId", std::get<0>(toRem));

        try {
            Lock lock(m_conn_lock);
            q1.execute();

        } catch (std::exception& e) {

            throw std::runtime_error("database error - " + std::string(e.what()));
        }
    }
}

static void addFilter(
    std::stringstream& qs, const std::string& filter, const std::vector<std::string>& values)
{
    auto value = values.begin();
    qs << " ( ";
    qs << filter << " = " << *value << " ";

    for (auto it = std::next(values.begin()); it != values.end(); it++) {
        qs << " OR ";
        qs << filter << " = " << *it << " ";
    }
    qs << " ) ";
}

std::vector<std::string> DB::listAssets(std::map<std::string, std::vector<std::string>> filters)
{
    std::vector<std::string> assetList;

    tntdb::Statement q;

    std::stringstream qs;

    qs << " SELECT "
          " name AS name "
          " FROM t_bios_asset_element ";

    if (!filters.empty()) {
        qs << " WHERE ";

        // first filter
        auto it     = filters.begin();
        auto filter = it->first;
        auto values = it->second;

        addFilter(qs, filter, values);

        // next filters
        for (it = std::next(filters.begin()); it != filters.end(); it++) {
            qs << " AND ";
            filter = it->first;
            values = it->second;

            addFilter(qs, filter, values);
        }
    }

    q = m_conn.prepareCached(qs.str().c_str());

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    for (const auto& row : res) {
        const std::string& assetName = row.getString("name");
        // discard rackcontroller 0
        if (assetName != RC0) {
            assetList.emplace_back(assetName);
        }
    }

    return assetList;
}

std::vector<std::string> DB::listAllAssets()
{
    std::vector<std::string> assetList;

    // clang-format off
    auto q = m_conn.prepareCached(R"(
        SELECT
            name          AS name
        FROM t_bios_asset_element
    )");
    // clang-format on

    tntdb::Result res;

    try {
        Lock lock(m_conn_lock);
        res = q.select();

    } catch (std::exception& e) {

        throw std::runtime_error("database error - " + std::string(e.what()));
    }

    for (const auto& row : res) {
        const std::string& assetName = row.getString("name");
        // discard rackcontroller 0
        if (assetName != RC0) {
            assetList.emplace_back(assetName);
        }
    }

    return assetList;
}

} // namespace fty
