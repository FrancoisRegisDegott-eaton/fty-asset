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

#pragma once
#include "asset-storage.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tntdb.h>
#include <vector>

namespace fty {

class DB : public AssetStorage
{
public:
    static DB& getInstance();

    void loadAsset(const std::string& nameId, Asset& asset);

    void                     loadExtMap(Asset& asset);
    void                     loadLinkedAssets(Asset& asset);
    std::vector<std::string> getChildren(const Asset& asset);

    fty::Expected<uint32_t> getID(const std::string& internalName);
    uint32_t getTypeID(const std::string& type);
    uint32_t getSubtypeID(const std::string& subtype);
    bool verifyID(std::string& id);

    uint32_t getLinkID(const uint32_t destId, const AssetLink& l);
    void saveLink(const uint32_t destId, const AssetLink& l);
    void removeLink(const uint32_t destId, const AssetLink& l);

    bool hasLinkedAssets(const Asset& asset);
    void unlinkAll(Asset& dest);
    void loadLinkExtMap(const uint32_t linkID, AssetLink& link);
    void saveLinkExtMap(const uint32_t linkID, const AssetLink& link);
    void clearGroup(Asset& asset);
    void removeAsset(Asset& asset);
    void removeFromRelations(Asset& asset);
    void removeFromGroups(Asset& asset);
    void removeExtMap(Asset& asset);
    bool isLastDataCenter(Asset& asset);

    void beginTransaction();
    void rollbackTransaction();
    void commitTransaction();

    void update(Asset& asset);
    void insert(Asset& asset);

    void        saveLinkedAssets(Asset& asset);
    void        saveExtMap(Asset& asset);
    std::string inameById(uint32_t id);
    std::string inameByUuid(const std::string& uuid);

    std::vector<std::string> listAssets(std::map<std::string, std::vector<std::string>> filters);
    std::vector<std::string> listAllAssets();

private:
    DB();
    void makeName(std::string& name);
    std::mutex                m_conn_lock;
    mutable tntdb::Connection m_conn;
};

} // namespace fty
