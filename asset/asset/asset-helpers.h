#pragma once
#include "error.h"
#include <fty/expected.h>
#include <string>
#include <uuid/uuid.h>

namespace fty {
class FullAsset;
}

namespace fty::asset {

  static constexpr auto UUID_TYPE_VERSION_NIL = 0;
  static constexpr auto UUID_TYPE_VERSION_4 = UUID_TYPE_DCE_RANDOM;
  static constexpr auto UUID_TYPE_VERSION_5 = UUID_TYPE_DCE_SHA1;

  struct AssetFilter
  {
      AssetFilter(const std::string& _manufacturer, const std::string& _model, const std::string& _serial, const std::string& _ipAddr = {})
       : manufacturer(_manufacturer)
       , model(_model)
       , serial(_serial)
       , ipAddr(_ipAddr)
       {}

      std::string manufacturer;
      std::string model;
      std::string serial;
      std::string ipAddr;
  };

  struct Uuid
  {
      Uuid(const std::string& _uuid = "", const int _type = UUID_TYPE_VERSION_NIL)
       : uuid(_uuid)
       , type(_type)
       {}

      std::string uuid;
      int         type;
  };

  AssetExpected<uint32_t>    checkElementIdentifier(const std::string& paramName, const std::string& paramValue);
  AssetExpected<std::string> sanitizeDate(const std::string& inp);
  AssetExpected<double>      sanitizeValueDouble(const std::string& key, const std::string& value);
  AssetExpected<void>        tryToPlaceAsset(uint32_t id, uint32_t parentId, uint32_t size, uint32_t loc);
  AssetExpected<void>        checkDuplicatedAsset(const AssetFilter& assetFilter);
  Uuid                       generateUUID(const AssetFilter& assetFilter);

  namespace activation {
      AssetExpected<bool> isActivable(const FullAsset& asset);
      AssetExpected<void> activate(const FullAsset& asset);
      AssetExpected<void> deactivate(const FullAsset& asset);
      AssetExpected<bool> isActivable(const std::string& assetJson);
      AssetExpected<void> activate(const std::string& assetJson);
      AssetExpected<void> deactivate(const std::string& assetJson);
  } // namespace activation

} // namespace fty::asset
