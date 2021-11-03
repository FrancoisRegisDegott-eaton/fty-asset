/*  =========================================================================
    fty_asset_dto - Asset DTO: defines ASSET attributes for data exchange

    Copyright (C) 2016 - 2020 Eaton

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
    fty_asset_dto - Asset DTO: defines ASSET attributes for data exchange
@discuss
@end
*/

#include "fty_asset_dto.h"
#include "conversion/full-asset.h"
#include "conversion/json.h"
#include "conversion/proto.h"
#include <algorithm>
#include <cxxtools/jsondeserializer.h>
#include <cxxtools/jsonserializer.h>
#include <fty_proto.h>
#include <sstream>


namespace fty {

AssetLink::AssetLink(const std::string& s, std::string o, std::string i, int t)
    : m_sourceId(s)
    , m_srcOut(o)
    , m_destIn(i)
    , m_linkType(t)
{
}

static constexpr const char* SI_LINK_SOURCE       = "source";
static constexpr const char* SI_LINK_TYPE         = "link_type";
static constexpr const char* SI_LINK_SRC_OUT      = "src_out";
static constexpr const char* SI_LINK_DEST_IN      = "dest_in";
static constexpr const char* SI_LINK_EXT          = "link_ext";
static constexpr const char* SI_LINK_SECONDARY_ID = "secondary_id";

const std::string& AssetLink::sourceId() const
{
    return m_sourceId;
}

const std::string& AssetLink::srcOut() const
{
    return m_srcOut;
}

const std::string& AssetLink::destIn() const
{
    return m_destIn;
}

int AssetLink::linkType() const
{
    return m_linkType;
}

const AssetLink::ExtMap& AssetLink::ext() const
{
    return m_ext;
}

const std::string& AssetLink::extEntry(const std::string& key) const
{
    static const std::string extNotFound;

    auto search = m_ext.find(key);

    if (search != m_ext.end()) {
        return search->second.getValue();
    }

    return extNotFound;
}

bool AssetLink::isReadOnly(const std::string& key) const
{
    auto search = m_ext.find(key);

    if (search != m_ext.end()) {
        return search->second.isReadOnly();
    }

    return false;
}

const std::string& AssetLink::secondaryID() const
{
    return m_secondaryID;
}

void AssetLink::setSourceId(const std::string& sourceId)
{
    m_sourceId = sourceId;
}

void AssetLink::setSrcOut(const std::string& srcOut)
{
    m_srcOut = srcOut;
}

void AssetLink::setDestIn(const std::string& destIn)
{
    m_destIn = destIn;
}

void AssetLink::setLinkType(const int linkType)
{
    m_linkType = linkType;
}

void AssetLink::setExt(const AssetLink::ExtMap& ext)
{
    m_ext = ext;
}

void AssetLink::setExtEntry(const std::string& key, const std::string& value, bool readOnly, bool forceUpdatedFalse)
{
    if (m_ext.find(key) != m_ext.end()) {
        // key already exists, update values
        m_ext.at(key).setValue(value);
        m_ext.at(key).setReadOnly(readOnly);
    } else {
        ExtMapElement element(value, readOnly, forceUpdatedFalse);
        m_ext[key] = element;
    }
}

void AssetLink::setSecondaryID(const std::string& secondaryID)
{
    m_secondaryID = secondaryID;
}

void AssetLink::clearExtMap()
{
    m_ext.clear();
}

void AssetLink::serialize(cxxtools::SerializationInfo& si) const
{
    si.addMember(SI_LINK_SOURCE) <<= m_sourceId;
    si.addMember(SI_LINK_TYPE) <<= m_linkType;
    if (!m_srcOut.empty()) {
        si.addMember(SI_LINK_SRC_OUT) <<= m_srcOut;
    }
    if (!m_destIn.empty()) {
        si.addMember(SI_LINK_DEST_IN) <<= m_destIn;
    }

    if (!m_ext.empty()) {
        cxxtools::SerializationInfo& ext = si.addMember("");
        cxxtools::SerializationInfo  data;
        for (const auto& e : m_ext) {
            cxxtools::SerializationInfo& entry = data.addMember(e.first);
            entry <<= e.second;
        }
        data.setCategory(cxxtools::SerializationInfo::Category::Object);
        ext = data;
        ext.setName(SI_LINK_EXT);
    }

    if (!m_secondaryID.empty()) {
        si.addMember(SI_LINK_SECONDARY_ID) <<= m_secondaryID;
    }
}

void AssetLink::deserialize(const cxxtools::SerializationInfo& si)
{
    si.getMember(SI_LINK_SOURCE) >>= m_sourceId;
    si.getMember(SI_LINK_TYPE) >>= m_linkType;
    if (si.findMember(SI_LINK_SRC_OUT) != NULL) {
        si.getMember(SI_LINK_SRC_OUT) >>= m_srcOut;
    }
    if (si.findMember(SI_LINK_DEST_IN) != NULL) {
        si.getMember(SI_LINK_DEST_IN) >>= m_destIn;
    }

    // ext map
    m_ext.clear();
    if (si.findMember(SI_LINK_EXT) != NULL) {
        const cxxtools::SerializationInfo ext = si.getMember(SI_LINK_EXT);
        for (const auto& si_link_ext : ext) {
            std::string   key = si_link_ext.name();
            ExtMapElement element;
            si_link_ext >>= element;
            m_ext[key] = element;
        }
    }

    if (si.findMember(SI_LINK_SECONDARY_ID) != NULL) {
        si.getMember(SI_LINK_SECONDARY_ID) >>= m_secondaryID;
    }
}

bool operator==(const AssetLink& l, const AssetLink& r)
{
    // note that the external map of attributes does not determine if two links are equal
    return ((l.sourceId() == r.sourceId()) && (l.srcOut() == r.srcOut()) && (l.destIn() == r.destIn()) && (l.linkType() == r.linkType()));
}

void operator<<=(cxxtools::SerializationInfo& si, const AssetLink& l)
{
    l.serialize(si);
}

void operator>>=(const cxxtools::SerializationInfo& si, AssetLink& l)
{
    l.deserialize(si);
}


const std::string assetStatusToString(AssetStatus status)
{
    std::string retVal;

    switch (status) {
        case AssetStatus::Active:
            retVal = "active";
            break;
        case AssetStatus::Nonactive:
            retVal = "nonactive";
            break;
        case AssetStatus::Unknown:
        default:
            retVal = "unknown";
            break;
    }

    return retVal;
}

AssetStatus stringToAssetStatus(const std::string& str)
{
    AssetStatus retVal = AssetStatus::Unknown;

    if (str == "active") {
        retVal = AssetStatus::Active;
    } else if (str == "nonactive") {
        retVal = AssetStatus::Nonactive;
    }

    return retVal;
}

// getters

const std::string& Asset::getInternalName() const
{
    return m_internalName;
}

AssetStatus Asset::getAssetStatus() const
{
    return m_assetStatus;
}

const std::string& Asset::getAssetType() const
{
    return m_assetType;
}

const std::string& Asset::getAssetSubtype() const
{
    return m_assetSubtype;
}

const std::string& Asset::getParentIname() const
{
    return m_parentIname;
}

int Asset::getPriority() const
{
    return m_priority;
}

const std::string& Asset::getAssetTag() const
{
    return m_assetTag;
}

const Asset::ExtMap& Asset::getExt() const
{
    return m_ext;
}

const std::string& Asset::getSecondaryID() const
{
    return m_secondaryID;
}

const std::string& Asset::getExtEntry(const std::string& key) const
{
    static const std::string extNotFound;

    auto search = m_ext.find(key);

    if (search != m_ext.end()) {
        return search->second.getValue();
    }

    return extNotFound;
}

bool Asset::isExtEntryReadOnly(const std::string& key) const
{
    auto search = m_ext.find(key);

    if (search != m_ext.end()) {
        return search->second.isReadOnly();
    }

    return false;
}

// ext param getters
const std::string& Asset::getUuid() const
{
    return getExtEntry(EXT_UUID);
}

const std::string& Asset::getManufacturer() const
{
    return getExtEntry(EXT_MANUFACTURER);
}

const std::string& Asset::getModel() const
{
    return getExtEntry(EXT_MODEL);
}

const std::string& Asset::getSerialNo() const
{
    return getExtEntry(EXT_SERIAL_NO);
}

const std::vector<AssetLink>& Asset::getLinkedAssets() const
{
    return m_linkedAssets;
}

bool Asset::hasParentsList() const
{
    return m_parentsList.has_value();
}

const std::vector<Asset>& Asset::getParentsList() const
{
    return m_parentsList.value();
}

// setters

void Asset::setInternalName(const std::string& internalName)
{
    m_internalName = internalName;
}

void Asset::setAssetStatus(AssetStatus assetStatus)
{
    m_assetStatus = assetStatus;
}

void Asset::setAssetType(const std::string& assetType)
{
    m_assetType = assetType;
}

void Asset::setAssetSubtype(const std::string& assetSubtype)
{
    m_assetSubtype = assetSubtype;
}

void Asset::setParentIname(const std::string& parentIname)
{
    m_parentIname = parentIname;
}

void Asset::setPriority(int priority)
{
    m_priority = priority;
}

void Asset::setAssetTag(const std::string& assetTag)
{
    m_assetTag = assetTag;
}

void Asset::setExtMap(const ExtMap& map)
{
    m_ext = map;
}

void Asset::clearExtMap()
{
    m_ext.clear();
}

void Asset::setExtEntry(const std::string& key, const std::string& value, bool readOnly, bool forceUpdatedFalse)
{
    if (m_ext.find(key) != m_ext.end()) {
        // key already exists, update values
        m_ext.at(key).setValue(value);
        m_ext.at(key).setReadOnly(readOnly);
    } else {
        ExtMapElement element(value, readOnly, forceUpdatedFalse);
        m_ext[key] = element;
    }
}

void Asset::addLink(
    const std::string& sourceId, const std::string& scrOut, const std::string& destIn, int linkType, const AssetLink::ExtMap& attributes)
{
    AssetLink l(sourceId, scrOut, destIn, linkType);
    l.setExt(attributes);

    m_linkedAssets.push_back(l);
}

void Asset::removeLink(const std::string& sourceId, const std::string& scrOut, const std::string& destIn, int linkType)
{
    AssetLink l(sourceId, scrOut, destIn, linkType);
    auto      found = std::find(m_linkedAssets.begin(), m_linkedAssets.end(), l);

    if (found != m_linkedAssets.end()) {
        m_linkedAssets.erase(found);
    }
}

void Asset::setLinkedAssets(const std::vector<AssetLink>& assets)
{
    m_linkedAssets = assets;
}

void Asset::setSecondaryID(const std::string& secondaryID)
{
    m_secondaryID = secondaryID;
}

// wrapper for address
Asset::AddressMap Asset::getAddressMap() const
{
    Asset::AddressMap addresses;

    for (uint16_t index = 0; index <= 255; index++) {
        const std::string& address = getAddress(static_cast<uint8_t>(index));

        if (!address.empty()) {
            addresses[static_cast<uint8_t>(index)] = address;
        }
    }

    return addresses;
}

const std::string& Asset::getAddress(uint8_t index) const
{
    return getExtEntry("ip." + std::to_string(index));
}

void Asset::setAddress(uint8_t index, const std::string& address)
{
    setExtEntry("ip." + std::to_string(index), address);
}

void Asset::removeAddress(uint8_t index)
{
    setAddress(index, "");
}

// Wrapper for Endpoints
Asset::ProtocolMap Asset::getProtocolMap() const
{
    Asset::ProtocolMap protocols;

    for (uint16_t index = 0; index <= 255; index++) {
        const std::string& protocol = getEndpointProtocol(static_cast<uint8_t>(index));

        if (!protocol.empty()) {
            protocols[static_cast<uint8_t>(index)] = protocol;
        }
    }

    return protocols;
}

const std::string& Asset::getEndpointProtocol(uint8_t index) const
{
    return getEndpointData(index, "protocol");
}
const std::string& Asset::getEndpointPort(uint8_t index) const
{
    return getEndpointData(index, "port");
}
const std::string& Asset::getEndpointSubAddress(uint8_t index) const
{
    return getEndpointData(index, "sub_address");
}
const std::string& Asset::getEndpointOperatingStatus(uint8_t index) const
{
    return getEndpointData(index, "status.operating");
}
const std::string& Asset::getEndpointErrorMessage(uint8_t index) const
{
    return getEndpointData(index, "status.error_msg");
}

const std::string& Asset::getEndpointProtocolAttribute(uint8_t index, const std::string& attributeName) const
{
    static std::string noProtocol;

    std::string protocol = getEndpointProtocol(index);
    if (protocol.empty()) {
        return noProtocol;
    }

    return getEndpointData(index, "." + protocol + "." + attributeName);
}

void Asset::setEndpointProtocol(uint8_t index, const std::string& val)
{
    return setEndpointData(index, "protocol", val);
}
void Asset::setEndpointPort(uint8_t index, const std::string& val)
{
    return setEndpointData(index, "port", val);
}
void Asset::setEndpointSubAddress(uint8_t index, const std::string& val)
{
    return setEndpointData(index, "sub_address", val);
}
void Asset::setEndpointOperatingStatus(uint8_t index, const std::string& val)
{
    return setEndpointData(index, "status.operating", val);
}
void Asset::setEndpointErrorMessage(uint8_t index, const std::string& val)
{
    return setEndpointData(index, "status.error_msg", val);
}

void Asset::setEndpointProtocolAttribute(uint8_t index, const std::string& attributeName, const std::string& val)
{
    std::string protocol = getEndpointProtocol(index);
    if (protocol.empty()) {
        return;
    }

    return setEndpointData(index, "." + protocol + "." + attributeName, val);
}

void Asset::removeEndpoint(uint8_t index)
{
    setEndpointProtocol(index, "");
    setEndpointPort(index, "");
    setEndpointSubAddress(index, "");
    setEndpointOperatingStatus(index, "");
    setEndpointErrorMessage(index, "");
}

const std::string& Asset::getEndpointData(uint8_t index, const std::string& field) const
{
    return getExtEntry("endpoint." + std::to_string(index) + "." + field);
}

void Asset::setEndpointData(uint8_t index, const std::string& field, const std::string& val)
{
    setExtEntry("endpoint." + std::to_string(index) + "." + field, val);
}

// friendly name
const std::string& Asset::getFriendlyName() const
{
    return getExtEntry("name");
}

void Asset::setFriendlyName(const std::string& friendlyName)
{
    setExtEntry("name", friendlyName);
}

void Asset::dump(std::ostream& os)
{
    os << "iname       : " << m_internalName << std::endl;
    os << "status      : " << assetStatusToString(m_assetStatus) << std::endl;
    os << "type        : " << m_assetType << std::endl;
    os << "subtype     : " << m_assetSubtype << std::endl;
    os << "parent      : " << m_parentIname << std::endl;
    os << "priority    : " << m_priority << std::endl;
    os << "tag         : " << m_assetTag << std::endl;
    os << "secondaryID : " << m_secondaryID << std::endl;
    os << "ext attrib  : " << m_secondaryID << std::endl;

    for (const auto& e : m_ext) {
        os << "- key: " << e.first << " - value: " << e.second.getValue() << (e.second.isReadOnly() ? " [ReadOny]" : "")
           << (e.second.wasUpdated() ? " [Updated]" : "") << std::endl;
    }

    for (const auto& l : m_linkedAssets) {
        os << "- linked to: " << l.sourceId() << " ( link type : " << l.linkType() << ")";
        if (!l.srcOut().empty()) {
            os << " on port: " << l.srcOut();
        }
        if (!l.destIn().empty()) {
            os << " from port " << l.destIn() << std::endl;
        }

        os << "- ext attrib  : " << m_secondaryID << std::endl;

        for (const auto& e : l.ext()) {
            os << "  - key: " << e.first << " - value: " << e.second.getValue() << (e.second.isReadOnly() ? " [ReadOny]" : "")
               << (e.second.wasUpdated() ? " [Updated]" : "") << std::endl;
        }
    }
}

bool Asset::operator==(const Asset& asset) const
{
    return (
        m_internalName == asset.m_internalName && m_assetStatus == asset.m_assetStatus && m_assetType == asset.m_assetType &&
        m_assetSubtype == asset.m_assetSubtype && m_parentIname == asset.m_parentIname && m_priority == asset.m_priority &&
        m_assetTag == asset.m_assetTag && m_ext == asset.m_ext && m_linkedAssets == asset.m_linkedAssets);
}

bool Asset::operator!=(const Asset& asset) const
{
    return !(*this == asset);
}

static constexpr const char* SI_STATUS       = "status";
static constexpr const char* SI_TYPE         = "type";
static constexpr const char* SI_SUB_TYPE     = "sub_type";
static constexpr const char* SI_NAME         = "name";
static constexpr const char* SI_PRIORITY     = "priority";
static constexpr const char* SI_PARENT       = "parent";
static constexpr const char* SI_LINKED       = "linked";
static constexpr const char* SI_EXT          = "ext";
static constexpr const char* SI_SECONDARY_ID = "secondary_id";
static constexpr const char* SI_PARENTS_LIST = "parents_list";

void Asset::serialize(cxxtools::SerializationInfo& si) const
{
    si.addMember(SI_STATUS) <<= int(m_assetStatus);
    si.addMember(SI_TYPE) <<= m_assetType;
    si.addMember(SI_SUB_TYPE) <<= m_assetSubtype;
    si.addMember(SI_NAME) <<= m_internalName;
    si.addMember(SI_PRIORITY) <<= m_priority;
    si.addMember(SI_PARENT) <<= m_parentIname;

    // linked assets
    cxxtools::SerializationInfo& linked = si.addMember("");

    cxxtools::SerializationInfo tmpSiLinks;
    for (const auto& l : m_linkedAssets) {
        cxxtools::SerializationInfo& link = tmpSiLinks.addMember("");
        link <<= l;
        link.setCategory(cxxtools::SerializationInfo::Category::Object);
    }
    tmpSiLinks.setCategory(cxxtools::SerializationInfo::Category::Array);
    linked = tmpSiLinks;
    linked.setName(SI_LINKED);

    // ext
    cxxtools::SerializationInfo& ext = si.addMember("");

    cxxtools::SerializationInfo data;
    for (const auto& e : m_ext) {
        cxxtools::SerializationInfo& entry = data.addMember(e.first);
        entry <<= e.second;
    }
    data.setCategory(cxxtools::SerializationInfo::Category::Object);
    ext = data;
    ext.setName(SI_EXT);

    if (!m_secondaryID.empty()) {
        si.addMember(SI_SECONDARY_ID) <<= m_secondaryID;
    }

    if (m_parentsList.has_value()) {
        si.addMember(SI_PARENTS_LIST) <<= m_parentsList.value();
    }
}

void Asset::deserialize(const cxxtools::SerializationInfo& si)
{
    int tmpInt = 0;

    si.getMember(SI_STATUS) >>= tmpInt;
    m_assetStatus = AssetStatus(tmpInt);

    si.getMember(SI_TYPE) >>= m_assetType;
    si.getMember(SI_SUB_TYPE) >>= m_assetSubtype;
    si.getMember(SI_NAME) >>= m_internalName;
    si.getMember(SI_PRIORITY) >>= m_priority;
    si.getMember(SI_PARENT) >>= m_parentIname;

    // linked assets
    const cxxtools::SerializationInfo linked = si.getMember(SI_LINKED);
    for (const auto& link_si : linked) {
        AssetLink l;
        link_si >>= l;

        m_linkedAssets.push_back(l);
    }

    // ext map
    m_ext.clear();
    const cxxtools::SerializationInfo ext = si.getMember(SI_EXT);
    for (const auto& siExt : ext) {
        std::string   key = siExt.name();
        ExtMapElement element;
        siExt >>= element;
        m_ext[key] = element;
    }

    if (si.findMember(SI_SECONDARY_ID) != NULL) {
        si.getMember(SI_SECONDARY_ID) >>= m_secondaryID;
    }

    if (si.findMember(SI_PARENTS_LIST) != nullptr) {
        std::vector<Asset> parentsList;
        si.getMember(SI_PARENTS_LIST) >>= parentsList;
        m_parentsList = parentsList;
    }
}

void Asset::fromJson(const std::string& json, Asset& a)
{
    conversion::fromJson(json, a);
}

void Asset::fromFtyProto(fty_proto_t* p, Asset& a, bool extAttributeReadOnly, bool test)
{
    conversion::fromFtyProto(p, a, extAttributeReadOnly, test);
}

FullAsset Asset::toFullAsset(const Asset& a)
{
    return conversion::toFullAsset(a);
}

std::string Asset::toJson(const Asset& a)
{
    return conversion::toJson(a);
}

fty_proto_t* Asset::toFtyProto(const Asset& a, const std::string& op, bool test)
{
    return conversion::toFtyProto(a, op, test);
}


void operator<<=(cxxtools::SerializationInfo& si, const fty::Asset& asset)
{
    asset.serialize(si);
}

void operator>>=(const cxxtools::SerializationInfo& si, fty::Asset& asset)
{
    asset.deserialize(si);
}

ExtMapElement::ExtMapElement(const std::string& val, bool readOnly, bool forceToFalse)
{
    setValue(val);
    setReadOnly(readOnly);

    if (forceToFalse) {
        m_wasUpdated = false;
    }
}

ExtMapElement::ExtMapElement(const ExtMapElement& element)
{
    m_value      = element.m_value;
    m_readOnly   = element.m_readOnly;
    m_wasUpdated = element.m_wasUpdated;
}

ExtMapElement::ExtMapElement(ExtMapElement&& element)
{
    m_value      = element.m_value;
    m_readOnly   = element.m_readOnly;
    m_wasUpdated = element.m_wasUpdated;

    element.m_value      = std::string();
    element.m_readOnly   = false;
    element.m_wasUpdated = false;
}

ExtMapElement& ExtMapElement::operator=(const ExtMapElement& element)
{
    m_value      = element.m_value;
    m_readOnly   = element.m_readOnly;
    m_wasUpdated = element.m_wasUpdated;
    return *this;
}

ExtMapElement& ExtMapElement::operator=(ExtMapElement&& element)
{
    m_value      = element.m_value;
    m_readOnly   = element.m_readOnly;
    m_wasUpdated = element.m_wasUpdated;
    return *this;
}

const std::string& ExtMapElement::getValue() const
{
    return m_value;
}

bool ExtMapElement::isReadOnly() const
{
    return m_readOnly;
}

bool ExtMapElement::wasUpdated() const
{
    return m_wasUpdated;
}

void ExtMapElement::setValue(const std::string& val)
{
    m_wasUpdated |= (m_value != val);
    m_value = val;
}

void ExtMapElement::setReadOnly(bool readOnly)
{
    m_wasUpdated |= (m_readOnly != readOnly);
    m_readOnly = readOnly;
}

bool ExtMapElement::operator==(const ExtMapElement& element) const
{
    return (m_value == element.m_value && m_readOnly == element.m_readOnly);
}

bool ExtMapElement::operator!=(const ExtMapElement& element) const
{
    return !(*this == element);
}

static constexpr const char* SI_EXT_MAP_ELEMENT_VALUE    = "value";
static constexpr const char* SI_EXT_MAP_ELEMENT_READONLY = "readOnly";
static constexpr const char* SI_EXT_MAP_ELEMENT_UPDATED  = "update";

// serialization / deserialization for cxxtools
void ExtMapElement::serialize(cxxtools::SerializationInfo& si) const
{
    si.addMember(SI_EXT_MAP_ELEMENT_VALUE) <<= m_value;
    si.addMember(SI_EXT_MAP_ELEMENT_READONLY) <<= m_readOnly;
    si.addMember(SI_EXT_MAP_ELEMENT_UPDATED) <<= m_wasUpdated;
}

void ExtMapElement::deserialize(const cxxtools::SerializationInfo& si)
{
    si.getMember(SI_EXT_MAP_ELEMENT_VALUE) >>= m_value;
    si.getMember(SI_EXT_MAP_ELEMENT_READONLY) >>= m_readOnly;
    si.getMember(SI_EXT_MAP_ELEMENT_UPDATED) >>= m_wasUpdated;
}

void operator<<=(cxxtools::SerializationInfo& si, const ExtMapElement& e)
{
    e.serialize(si);
}

void operator>>=(const cxxtools::SerializationInfo& si, ExtMapElement& e)
{
    e.deserialize(si);
}


UIAsset::UIAsset(const Asset& a)
    : Asset(a)
{
}

static constexpr const char* UI_ASSET_ID            = "id";
static constexpr const char* UI_ASSET_LOCATION      = "location_id";
static constexpr const char* UI_ASSET_LOCATION_URI  = "location_uri";
static constexpr const char* UI_ASSET_FRIENDLY_NAME = "name";
static constexpr const char* UI_TYPE                = "type";
static constexpr const char* UI_SUBTYPE             = "sub_type";
static constexpr const char* UI_PRIORITY            = "priority";

void UIAsset::serializeUI(cxxtools::SerializationInfo& si) const
{
    si.addMember(UI_ASSET_ID) <<= m_internalName;

    if (!m_parentIname.empty()) {
        si.addMember(UI_ASSET_LOCATION) <<= m_parentIname;
        si.addMember(UI_ASSET_LOCATION) <<= ("/api/v1/asset/" + m_parentIname);
    }

    si.addMember(UI_ASSET_FRIENDLY_NAME) <<= getFriendlyName();

    si.addMember(UI_TYPE) <<= m_assetType;
    si.addMember(UI_SUBTYPE) <<= m_assetSubtype;

    si.addMember(UI_PRIORITY) <<= "P" + std::to_string(m_priority);


    /*si.addMember(SI_LINKED) <<= m_linkedAssets;

    // ext
    cxxtools::SerializationInfo& ext = si.addMember("");

    cxxtools::SerializationInfo data;
    for (const auto& e : m_ext) {
        cxxtools::SerializationInfo& entry = data.addMember(e.first);
        entry.addMember("value") <<= e.second.getValue();
        entry.addMember("readOnly") <<= e.second.isReadOnly();
        entry.addMember("updated") <<= e.second.wasUpdated();
    }
    data.setCategory(cxxtools::SerializationInfo::Category::Object);
    ext = data;
    ext.setName(SI_EXT);

    if (m_parentsList.has_value()) {
        si.addMember(SI_PARENTS_LIST) <<= m_parentsList.value();
    }*/
}

void UIAsset::deserializeUI(const cxxtools::SerializationInfo& /*si*/)
{
}


} // namespace fty
