#pragma once
#include "error.h"
#include <fty/expected.h>
#include <string>

namespace fty {
class FullAsset;
}

namespace fty::asset {

AssetExpected<uint32_t>    checkElementIdentifier(const std::string& paramName, const std::string& paramValue);
AssetExpected<std::string> sanitizeDate(const std::string& inp);
AssetExpected<double>      sanitizeValueDouble(const std::string& key, const std::string& value);
AssetExpected<void>        tryToPlaceAsset(uint32_t id, uint32_t parentId, uint32_t size, uint32_t loc);

namespace activation {
    AssetExpected<bool> isActivable(const FullAsset& asset);
    AssetExpected<void> activate(const FullAsset& asset);
    AssetExpected<void> deactivate(const FullAsset& asset);
    AssetExpected<bool> isActivable(const std::string& assetJson);
    AssetExpected<void> activate(const std::string& assetJson);
    AssetExpected<void> deactivate(const std::string& assetJson);
} // namespace activation

AssetExpected<std::string> normName(const std::string& name, int32_t assetId = -1);

} // namespace fty::asset
