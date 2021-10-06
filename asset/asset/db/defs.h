#pragma once
#include <functional>
#include <map>
#include <string>

namespace fty::db {
class Row;
} // namespace fty::db

namespace fty::db::asset {

struct ExtAttrValue
{
    std::string value;
    bool        readOnly = false;
};

using Attributes = std::map<std::string, ExtAttrValue>;

struct AssetItem
{
public:
    uint32_t    id = 0;
    std::string name;
    std::string status;
    uint32_t    parentId  = 0;
    uint16_t    priority  = 0;
    uint16_t    typeId    = 0;
    uint16_t    subtypeId = 0;
    std::string assetTag;
};

struct AssetItemExt : public AssetItem
{
    std::string extName;
    std::string typeName;
    uint16_t    parentTypeId = 0;
    std::string subtypeName;
    std::string parentName;
};

struct AssetLink
{
    uint32_t    srcId;
    uint32_t    destId;
    std::string srcName;
    std::string srcSocket;
    std::string destSocket;
};


} // namespace fty::db::asset
