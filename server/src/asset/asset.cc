/*  =========================================================================
    asset - asset

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

#include "asset.h"
#include "asset-cam.h"
#include "asset-db-test.h"
#include "asset-db.h"
#include "asset-storage.h"
#include "asset/dbhelpers.h"
#include <asset/asset-helpers.h>
#include <fty_common_mlm.h>
#include <fty_common.h>
#include <fty_common_db_dbpath.h>
#include <fty_log.h>
#include <fty/string-utils.h>
#include <fty_common_agents.h>
#include <map>

#define AGENT_ASSET_ACTIVATOR "etn-licensing-credits"

#define MAX_CREATE_RETRY 10

#define COMMAND_IS_ASSET_ACTIVABLE  "GET_IS_ASSET_ACTIVABLE"
#define COMMAND_ACTIVATE_ASSET      "ACTIVATE_ASSET"
#define COMMAND_DEACTIVATE_ASSET    "DEACTIVATE_ASSET"

namespace fty {

  using namespace fty::asset;

//============================================================================================================

template <typename T, typename T1>
bool isAnyOf(const T& val, const T1& other)
{
    return val == other;
}

template <typename T, typename T1, typename... Vals>
bool isAnyOf(const T& val, const T1& other, const Vals&... args)
{
    if (val == other) {
        return true;
    } else {
        return isAnyOf(val, args...);
    }
}

//============================================================================================================

/// generate current timestamp string in format yyyy-mm-ddThh:MM:ss+0000
static std::string generateCurrentTimestamp()
{
    static constexpr int tmpSize   = 100;
    std::time_t          timestamp = std::time(NULL);
    char                 timeString[tmpSize];
    std::strftime(timeString, tmpSize - 1, "%FT%T%z", std::localtime(&timestamp));

    return std::string(timeString);
}

static AssetStorage& getStorage()
{
    if (g_testMode) {
        return DBTest::getInstance();
    } else {
        return DB::getInstance();
    }
}

/// get children of Asset a
std::vector<std::string> getChildren(const AssetImpl& a)
{
    return getStorage().getChildren(a);
}

//============================================================================================================

// extract asset filters from SerializationInfo structure
void operator>>=(const cxxtools::SerializationInfo& si, AssetFilters& filters)
{
    try {
        if (si.findMember("status") != NULL) {
            const cxxtools::SerializationInfo& status = si.getMember("status");

            std::vector<std::string> v;
            status >>= v;

            for (const std::string& val : v) {
                filters["status"].push_back('"' + val + '"');
            }
        }
    } catch (const std::exception& e) {
        log_error("Invalid filter for status: %s", e.what());
    }

    try {
        if (si.findMember("type") != NULL) {
            const cxxtools::SerializationInfo& type = si.getMember("type");

            std::vector<std::string> v;
            type >>= v;

            for (const std::string& val : v) {
                uint32_t typeId = getStorage().getTypeID(val);

                if (typeId) {
                    filters["id_type"].push_back(std::to_string(typeId));
                } else {
                    log_error("Invalid type filter: %s type does not exist", val.c_str());
                }
            }
        }
    } catch (std::exception& e) {
        log_error("Invalid filter type: %s", e.what());
    }

    try {
        if (si.findMember("sub_type") != NULL) {
            const cxxtools::SerializationInfo& subtype = si.getMember("sub_type");

            std::vector<std::string> v;
            subtype >>= v;

            for (const std::string& val : v) {
                uint32_t subtypeId = getStorage().getSubtypeID(val);
                if (subtypeId) {
                    filters["id_subtype"].push_back(std::to_string(subtypeId));
                } else {
                    log_error("Invalid subtype filter: %s subtype does not exist", val.c_str());
                }
            }
        }
    } catch (std::exception& e) {
        log_error("Invalid filter sub_type: %s", e.what());
    }

    try {
        if (si.findMember("priority") != NULL) {
            const cxxtools::SerializationInfo& priority = si.getMember("priority");

            std::vector<int> v;
            priority >>= v;

            for (int val : v) {
                filters["priority"].push_back(std::to_string(val));
            }
        }
    } catch (std::exception& e) {
        log_error("Invalid filter priority: %s", e.what());
    }

    try {
        if (si.findMember("parent") != NULL) {
            const cxxtools::SerializationInfo& parent = si.getMember("parent");

            std::vector<std::string> v;
            parent >>= v;

            for (const std::string& val : v) {
                auto parentId = getStorage().getID(val);
                if (parentId) {
                    filters["id_parent"].push_back(std::to_string(*parentId));
                } else {
                    log_error("Invalid parent filter: %s does not exist", val.c_str());
                }
            }
        }
    } catch (std::exception& e) {
        log_error("Invalid filter parent: %s", e.what());
    }
}

//============================================================================================================

AssetImpl::AssetImpl()
    : m_storage(getStorage())
{
}

AssetImpl::AssetImpl(const std::string& nameId)
    : m_storage(getStorage())
{
    m_storage.loadAsset(nameId, *this);
    m_storage.loadExtMap(*this);
    m_storage.loadLinkedAssets(*this);
}

AssetImpl::~AssetImpl()
{
}

AssetImpl::AssetImpl(const AssetImpl& a)
    : Asset(a)
    , m_storage(getStorage())
{
}

AssetImpl& AssetImpl::operator=(const AssetImpl& a)
{
    if (&a == this) {
        return *this;
    }
    Asset::operator=(a);

    return *this;
}

bool AssetImpl::hasLogicalAsset() const
{
    return getExt().find("logical_asset") != getExt().end();
}

bool AssetImpl::isVirtual() const
{

    return ((getAssetType() == TYPE_INFRA_SERVICE) || (getAssetType() == TYPE_CLUSTER) || (getAssetType() == TYPE_HYPERVISOR) ||
            (getAssetType() == TYPE_VIRTUAL_MACHINE) || (getAssetType() == TYPE_STORAGE_SERVICE) ||
            (getAssetType() == TYPE_VAPP) || (getAssetType() == TYPE_CONNECTOR) ||
            (getAssetType() == TYPE_SERVER) || (getAssetType() == TYPE_PLANNER) ||
            (getAssetType() == TYPE_OPERATING_SYSTEM) || (getAssetType() == TYPE_PLAN));
}

bool AssetImpl::hasLinkedAssets() const
{
    return m_storage.hasLinkedAssets(*this);
}

void AssetImpl::remove(bool removeLastDC)
{
    if (RC0 == getInternalName()) {
        throw std::runtime_error("cannot delete RC-0");
    }

    // TODO verify if it is necessary to prevent assets with logical_asset attribute from being deleted
    // if (hasLogicalAsset()) {
    //     throw std::runtime_error("a logical_asset (sensor) refers to it");
    // }

    if (hasLinkedAssets()) {
        throw std::runtime_error("it the source of a link");
    }

    std::vector<std::string> assetChildren = getChildren(*this);

    if (!assetChildren.empty()) {
        throw std::runtime_error("it has at least one child");
    }

    // deactivate asset
    if (getAssetStatus() == AssetStatus::Active) {
        deactivate();
    }

    m_storage.beginTransaction();
    try {
        const std::string internalName = m_internalName;
        if (isAnyOf(getAssetType(), TYPE_DATACENTER, TYPE_ROW, TYPE_ROOM, TYPE_RACK)) {
            if (!removeLastDC && m_storage.isLastDataCenter(*this)) {
                throw std::runtime_error("cannot delete last datacenter");
            }
            m_storage.removeExtMap(*this);
            m_storage.removeFromGroups(*this);
            m_storage.removeFromRelations(*this);
            m_storage.removeAsset(*this);
        } else if (isAnyOf(getAssetType(), TYPE_GROUP)) {
            m_storage.clearGroup(*this);
            m_storage.removeExtMap(*this);
            m_storage.removeAsset(*this);
        } else if (isAnyOf(getAssetType(), TYPE_DEVICE)) {
            m_storage.removeFromGroups(*this);
            m_storage.unlinkAll(*this);
            m_storage.removeFromRelations(*this);
            m_storage.removeExtMap(*this);
            m_storage.removeAsset(*this);
        } else {
            m_storage.removeExtMap(*this);
            m_storage.removeAsset(*this);
        }

        log_debug("Asset %s removed", internalName.c_str());
    } catch (const std::exception& e) {
        m_storage.rollbackTransaction();

        // reactivate is previous status was active
        if (getAssetStatus() == AssetStatus::Active) {
            activate();
        }
        log_debug("Asset could not be removed: %s", e.what());
        throw std::runtime_error("Asset could not be removed: " + std::string(e.what()));
    }
    m_storage.commitTransaction();
}

static std::string generateRandomID()
{
    timeval t;
    gettimeofday(&t, NULL);
    srand(static_cast<unsigned int>(t.tv_sec * t.tv_usec));
    // generate 8 digit random integer
    unsigned long index = static_cast<unsigned long>(rand()) % static_cast<unsigned long>(100000000);

    std::string indexStr = std::to_string(index);

    // create 8 digit index with leading zeros
    indexStr = std::string(8 - indexStr.length(), '0') + indexStr;

    return indexStr;
}

// generate asset name
static std::string createAssetName(const std::string& type, const std::string& subtype, const std::string& id)
{
    std::string assetName;

    if (type == fty::TYPE_DEVICE) {
        assetName = subtype + "-" + id;
    } else {
        assetName = type + "-" + id;
    }

    return assetName;
}

void AssetImpl::create()
{
    m_storage.beginTransaction();
    try {
        if (!g_testMode) {
            std::string randomId;

            unsigned int retry = 0;
            bool valid = false;

            do {
                randomId = generateRandomID();
                valid = m_storage.verifyID(randomId);
                log_debug("Checking ID %s validty", randomId.c_str());

            } while (!valid && (retry++ < MAX_CREATE_RETRY));

            if(!valid) {
                throw std::runtime_error("Multiple Asset ID collisions - impossible to create asset");
            }

            std::string iname = createAssetName(getAssetType(), getAssetSubtype(), randomId);
            setInternalName(iname);
        }

        // set creation timestamp
        setExtEntry(fty::EXT_CREATE_TS, generateCurrentTimestamp(), true);
        // generate uuid if not already present in the payload
        if(getExtEntry("uuid").empty()) {
            AssetFilter assetFilter{getManufacturer(), getModel(), getSerialNo()};
            setExtEntry(fty::EXT_UUID, generateUUID(assetFilter).uuid, true);
        }

        m_storage.insert(*this);
        m_storage.saveLinkedAssets(*this);
        m_storage.saveExtMap(*this);

    } catch (const std::exception& e) {
        m_storage.rollbackTransaction();
        throw std::runtime_error(std::string(e.what()));
    }
    m_storage.commitTransaction();

    // create CAM mappings
    try {
        auto credentialList = getCredentialMappings(getExt());
        createMappings(getInternalName(), credentialList);
    } catch (const std::exception& e) {
        log_error("Failed to update CAM: %s", e.what());
    }

}

void AssetImpl::update()
{
    m_storage.beginTransaction();
    try {
        if (!g_testMode && !m_storage.getID(getInternalName())) {
            throw std::runtime_error("Update failed, asset does not exist.");
        }
        // set last update timestamp
        setExtEntry(fty::EXT_UPDATE_TS, generateCurrentTimestamp(), true);

        m_storage.update(*this);
        m_storage.saveLinkedAssets(*this);
        m_storage.saveExtMap(*this);
    } catch (const std::exception& e) {
        m_storage.rollbackTransaction();
        throw std::runtime_error(std::string(e.what()));
    }
    m_storage.commitTransaction();

    // update CAM mappings
    try {
        deleteMappings(getInternalName());
        auto credentialList = getCredentialMappings(getExt());
        createMappings(getInternalName(), credentialList);
    } catch (const std::exception& e) {
        log_error("Failed to update CAM: %s", e.what());
    }
}

void AssetImpl::restore(bool restoreLinks)
{
    m_storage.beginTransaction();
    // restore only if asset is not already in db
    if (m_storage.getID(getInternalName())) {
        throw std::runtime_error("Asset " + getInternalName() + " already exists, restore is not possible");
    }
    try {
        // set creation timestamp
        setExtEntry(fty::EXT_CREATE_TS, generateCurrentTimestamp(), true);

        m_storage.insert(*this);
        m_storage.saveExtMap(*this);
        if (restoreLinks) {
            m_storage.saveLinkedAssets(*this);
        }

    } catch (const std::exception& e) {
        m_storage.rollbackTransaction();
        log_debug("AssetImpl::save() got EXCEPTION : %s", e.what());
        throw e.what();
    }
    m_storage.commitTransaction();

    // create CAM mappings
    try {
        auto credentialList = getCredentialMappings(getExt());
        createMappings(getInternalName(), credentialList);
    } catch (const std::exception& e) {
        log_error("Failed to update CAM: %s", e.what());
    }
}

static std::vector<std::string> sendActivationReq(const std::string & command, const std::vector<std::string> & frames)
{
    mlm::MlmSyncClient client(AGENT_FTY_ASSET, AGENT_ASSET_ACTIVATOR);

    log_debug("Sending %s request to %s", command.c_str(), AGENT_ASSET_ACTIVATOR);

    std::vector<std::string> payload = {command};
    std::copy(frames.begin(), frames.end(), back_inserter(payload));

    std::vector<std::string> receivedFrames = client.syncRequestWithReply(payload);

    //check if the first frame we get is an error
    if(receivedFrames[0] == "ERROR")
    {
        //It's an error and we will throw directly the exceptions
        if(receivedFrames.size() == 2)
        {
            throw std::runtime_error(receivedFrames.at(1));
        }
        else
        {
            throw std::runtime_error("Missing data for error");
        }
    }

    return receivedFrames;
}

bool AssetImpl::isActivable()
{
    if (g_testMode) {
        return true;
    }

    bool ret = false;

    if (getAssetType() == TYPE_DEVICE) {
        try
        {
            auto payload = sendActivationReq(COMMAND_IS_ASSET_ACTIVABLE, {Asset::toFullAsset(*this).toJson()});
            std::istringstream isActivableStr (payload[0]);
            isActivableStr >> std::boolalpha >> ret;
            log_debug ("asset is activable = %s", payload[0].c_str ());
        }
        catch (const std::exception& e)
        {
            log_info ("Request failed: %s", e.what());
            return false;
        }
        return ret;
    } else {
        return true;
    }
}

void AssetImpl::activate()
{
    if (!g_testMode) {
        if (getAssetType() == TYPE_DEVICE) {
            try
            {
                auto payload = sendActivationReq(COMMAND_ACTIVATE_ASSET, {Asset::toFullAsset(*this).toJson()});
                setAssetStatus(fty::AssetStatus::Active);
                m_storage.update(*this);
                log_debug ("Asset %s activated", m_internalName.c_str());
            }
            catch (const std::exception& e)
            {
                log_error ("Asset %s activation failed", m_internalName.c_str());
            }
        } else {
            setAssetStatus(fty::AssetStatus::Active);
            m_storage.update(*this);
        }
    }
}

void AssetImpl::deactivate()
{
    if (!g_testMode) {
        if (getAssetType() == TYPE_DEVICE) {
            try
            {
                auto payload = sendActivationReq(COMMAND_DEACTIVATE_ASSET, {Asset::toFullAsset(*this).toJson()});
                setAssetStatus(fty::AssetStatus::Nonactive);
                m_storage.update(*this);
                log_debug ("Asset %s deactivated", m_internalName.c_str());
            }
            catch (const std::exception& e)
            {
                log_error ("Asset %s deactivation failed", m_internalName.c_str());
            }
        } else {
            setAssetStatus(fty::AssetStatus::Nonactive);
            m_storage.update(*this);
        }
    }
}

void AssetImpl::unlinkAll()
{
    m_storage.unlinkAll(*this);
}

static std::vector<fty::Asset> buildParentsList(const std::string iname)
{
    std::vector<fty::Asset> parents;

    fty::AssetImpl a(iname);

    while (!a.getParentIname().empty())
    {
        if (a.getParentIname() == a.getInternalName()) {
            log_error("Self parent detected (%s)", a.getInternalName().c_str());
            break;
        }

        a = fty::AssetImpl(a.getParentIname());
        parents.push_back(a);

        // secure, avoid infinite loop
        if (parents.size() > 32) break;
    }

    return parents;
}

void AssetImpl::updateParentsList()
{
    m_parentsList = buildParentsList(getInternalName());
}

void AssetImpl::assetToSrr(const AssetImpl& asset, cxxtools::SerializationInfo& si)
{
    // basic
    si.addMember("id") <<= asset.getInternalName();
    si.addMember("status") <<= int(asset.getAssetStatus());
    si.addMember("type") <<= asset.getAssetType();
    si.addMember("subtype") <<= asset.getAssetSubtype();
    si.addMember("priority") <<= asset.getPriority();
    si.addMember("parent") <<= asset.getParentIname();

    // linked assets
    cxxtools::SerializationInfo& linked = si.addMember("");

    cxxtools::SerializationInfo tmpSiLinks;
    for (const auto& l : asset.getLinkedAssets()) {
        cxxtools::SerializationInfo& link = tmpSiLinks.addMember("");
        link.addMember("source") <<= l.sourceId();

        if (!l.srcOut().empty()) {
            link.addMember("src_out") <<= l.srcOut();
        }
        if (!l.destIn().empty()) {
            link.addMember("dest_in") <<= l.destIn();
        }

        link.addMember("link_type") <<= l.linkType();

        if (!l.ext().empty()) {
            cxxtools::SerializationInfo& link_ext = link.addMember("");
            cxxtools::SerializationInfo  tmp_link_ext;
            for (const auto& e : l.ext()) {
                cxxtools::SerializationInfo& link_ext_entry = tmp_link_ext.addMember(e.first);
                link_ext_entry.addMember("value") <<= e.second.getValue();
                link_ext_entry.addMember("readOnly") <<= e.second.isReadOnly();
            }
            tmp_link_ext.setCategory(cxxtools::SerializationInfo::Category::Object);
            link_ext = tmp_link_ext;
            link_ext.setName("ext");
        }

        link.setCategory(cxxtools::SerializationInfo::Category::Object);
    }
    tmpSiLinks.setCategory(cxxtools::SerializationInfo::Category::Array);
    linked = tmpSiLinks;
    linked.setName("linked");

    si.addMember("tag") <<= asset.getAssetTag();
    si.addMember("id_secondary") <<= asset.getSecondaryID();

    // ext
    cxxtools::SerializationInfo& ext = si.addMember("");

    cxxtools::SerializationInfo tmpSiExt;
    for (const auto& e : asset.getExt()) {
        cxxtools::SerializationInfo& entry = tmpSiExt.addMember(e.first);
        entry.addMember("value") <<= e.second.getValue();
        entry.addMember("readOnly") <<= e.second.isReadOnly();
    }
    tmpSiExt.setCategory(cxxtools::SerializationInfo::Category::Object);
    ext = tmpSiExt;
    ext.setName("ext");
}

void AssetImpl::srrToAsset(const cxxtools::SerializationInfo& si, AssetImpl& asset)
{
    int         tmpInt = 0;
    std::string tmpString;

    // uuid
    si.getMember("id") >>= tmpString;
    asset.setInternalName(tmpString);

    // status
    si.getMember("status") >>= tmpInt;
    asset.setAssetStatus(AssetStatus(tmpInt));

    // type
    si.getMember("type") >>= tmpString;
    asset.setAssetType(tmpString);

    // subtype
    si.getMember("subtype") >>= tmpString;
    asset.setAssetSubtype(tmpString);

    // priority
    si.getMember("priority") >>= tmpInt;
    asset.setPriority(tmpInt);

    // parend id
    si.getMember("parent") >>= tmpString;
    asset.setParentIname(tmpString);

    // linked assets
    const cxxtools::SerializationInfo linked = si.getMember("linked");
    for (const auto& link_si : linked) {
        std::string key = link_si.name();
        std::string sourceId, srcOut, destIn;
        int linkType = 0;

        link_si.getMember("source") >>= sourceId;
        if(link_si.findMember("src_out") != NULL) {
            link_si.getMember("src_out") >>= srcOut;
        }
        if(link_si.findMember("dest_in") != NULL) {
            link_si.getMember("dest_in") >>= destIn;
        }
        link_si.getMember("link_type") >>= linkType;

        AssetLink::ExtMap linkAttributes;

        if(link_si.findMember("ext") != NULL) {
            // link ext map
            const cxxtools::SerializationInfo link_ext = link_si.getMember("ext");
            for (const auto& link_ext_si : link_ext) {
                std::string ext_key = link_ext_si.name();
                std::string val;
                bool        readOnly = false;
                link_ext_si.getMember("value") >>= val;
                link_ext_si.getMember("readOnly") >>= readOnly;

                ExtMapElement extElement(val, readOnly);

                linkAttributes[ext_key] = extElement;
            }
        }

        asset.addLink(sourceId, srcOut, destIn, linkType, linkAttributes);
    }

    // asset tag
    si.getMember("tag") >>= tmpString;
    asset.setAssetTag(tmpString);

    // id secondary
    si.getMember("id_secondary") >>= tmpString;
    asset.setSecondaryID(tmpString);

    // ext map
    const cxxtools::SerializationInfo ext = si.getMember("ext");

    for (const auto& siExt : ext) {
        std::string key = siExt.name();
        std::string val;
        bool        readOnly = false;
        siExt.getMember("value") >>= val;
        siExt.getMember("readOnly") >>= readOnly;

        asset.setExtEntry(key, val, readOnly);
    }
}

std::vector<std::string> AssetImpl::list(const AssetFilters& filters)
{
    return getStorage().listAssets(filters);
}

std::vector<std::string> AssetImpl::listAll()
{
    return getStorage().listAllAssets();
}

void AssetImpl::load()
{
    m_storage.loadAsset(getInternalName(), *this);
    m_storage.loadExtMap(*this);
    m_storage.loadLinkedAssets(*this);
}

static void addSubTree(const std::string& internalName, std::vector<AssetImpl>& toDel)
{
    std::vector<std::pair<AssetImpl, int>> stack;

    AssetImpl a(internalName);

    AssetImpl& ref = a;

    bool         end  = false;
    unsigned int next = 0;

    while (!end) {
        std::vector<std::string> children = getChildren(ref);

        if (next < children.size()) {
            stack.push_back(std::make_pair(ref, next + 1));

            ref  = children[next];
            next = 0;

            auto found = std::find_if(toDel.begin(), toDel.end(), [&](const AssetImpl& assetImpl) {
                return assetImpl.getInternalName() == ref.getInternalName();
            });
            if (found == toDel.end()) {
                toDel.push_back(ref);
            }
        } else {
            if (stack.empty()) {
                end = true;
            } else {
                ref  = stack.back().first;
                next = static_cast<unsigned int>(stack.back().second);
                stack.pop_back();
            }
        }
    }
}

DeleteStatus AssetImpl::deleteList(const std::vector<std::string>& assets, bool recursive, bool deleteVirtualAssets, bool removeLastDC)
{
    std::vector<AssetImpl> toDel;

    DeleteStatus deleted;

    for (const std::string& iname : assets) {
        try {
            AssetImpl a(iname);
            if(a.isVirtual() && !deleteVirtualAssets) {
                log_info("Asset %s is virtual, skipping delete...", a.getInternalName().c_str());
                continue;
            }
            toDel.push_back(a);

            if (recursive) {
                addSubTree(iname, toDel);
            }
        } catch (std::exception& e) {
            log_warning("Error while loading asset %s. %s", iname.c_str(), e.what());
        }
    }

    auto isAnyParent = [&](const AssetImpl& l, const AssetImpl& r) {
        std::vector<std::pair<std::string, int>> lTree;
        auto                                     ptr = l;
        // path to root of L
        int distL = 0;
        while (ptr.getParentIname() != "") {
            lTree.push_back(std::make_pair(ptr.getInternalName(), distL));
            std::string parentIname = ptr.getParentIname();
            ptr                     = AssetImpl(parentIname);
            distL++;
        }

        int distR = 0;
        ptr       = r;
        while (ptr.getParentIname() != "") {
            // look for LCA, if exists
            auto found = std::find_if(lTree.begin(), lTree.end(), [&](const std::pair<std::string, int>& p) {
                return p.first == ptr.getInternalName();
            });
            if (found != lTree.end()) {
                distL = found->second; // distance of L from LCA
                break;
            }

            ptr = AssetImpl(ptr.getParentIname());
            distR++;
        }
        // farthest node from LCA gets deleted first
        return distL > distR;
    };

    // sort by deletion order
    std::sort(toDel.begin(), toDel.end(), [&](const AssetImpl& l, const AssetImpl& r) {
        return isAnyParent(l, r);
    });

    // remove all links
    for (auto& a : toDel) {
        a.unlinkAll();
    }

    for (auto& d : toDel) {
        // after deleting assets, children list may change -> reload
        d.load();

        try {
            d.remove(removeLastDC);
            deleted.push_back({d, "OK"});

            // remove CAM mappings
            try {
                deleteMappings(d.getInternalName());
            } catch (const std::exception& e) {
                log_error("Failed to update CAM: %s", e.what());
            }
        } catch (std::exception& e) {
            log_error("Asset could not be removed: %s", e.what());
            deleted.push_back({d, "Asset could not be removed: " + std::string(e.what())});
        }
    }

    return deleted;
}

DeleteStatus AssetImpl::deleteAll(bool deleteVirtualAsset)
{
    // get list of all assets (including last datacenter)
    return deleteList(listAll(), false, deleteVirtualAsset, true);
}

/// get internal name from UUID
std::string AssetImpl::getInameFromUuid(const std::string& uuid)
{
    return getStorage().inameByUuid(uuid);
}

/// get internal database index from iname
uint32_t AssetImpl::getIDFromIname(const std::string& iname)
{
    log_debug("Request ID for asset %s", iname.c_str());
    auto id = getStorage().getID(iname);
    if(!id) {
        throw std::runtime_error(id.error());
    }
    return *id;
}

/// get internal name from database index
std::string AssetImpl::getInameFromID(const uint32_t id)
{
    log_debug("Request internal name for asset with ID %lu", id);
    std::string iname;
    try{
        iname = selectAssetProperty<std::string>("name", "id_asset_element", fty::convert<std::string>(id));
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
    return iname;
}

} // namespace fty
