#pragma once
#include "defs.h"
#include <fty/expected.h>
#include <set>

namespace fty::db {
class Connection;
class Row;
} // namespace fty::db

namespace fty::db::asset::select {

struct Filter
{
    std::vector<uint16_t> types;
    std::vector<uint16_t> subtypes;
    std::string           without;
    std::string           status;

    operator bool() const;
};

struct Order
{
    enum class Dir
    {
        Asc,
        Desc
    };

    std::string field;
    Dir         dir = Dir::Asc;

                                 operator bool() const;
    const std::set<std::string>& possibleOrders() const;
    bool                         isValid() const;
};

using Callback = std::function<void(const Row&)>;

// =========================================================================================================================================

/// select basic information about asset element by name
/// @param name asset internal or external name
/// @param extNameOnly select by external name only
/// @return AssetItem or error
[[nodiscard]] Expected<AssetItem> item(const std::string& name, bool extNameOnly = false);

/// select basic information about asset element by name
/// @param conn database established connection
/// @param name asset internal or external name
/// @param extNameOnly select by external name only
/// @return AssetItem or error
[[nodiscard]] Expected<AssetItem> item(Connection& conn, const std::string& name, bool extNameOnly = false);

// =========================================================================================================================================

/// Selects all assets
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<void> items(Callback&& cb, const Filter& filter = {}, const Order& order = {});

/// Selects all assets
/// @param conn database established connection
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<void> items(Connection& conn, Callback&& cb, const Filter& filter = {}, const Order& order = {});

// =========================================================================================================================================

/// Selects all assets
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<std::vector<AssetItemExt>> items(const Filter& filter = {}, const Order& order = {});

/// Selects all assets
/// @param conn database established connection
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<std::vector<AssetItemExt>> items(Connection& conn, const Filter& filter = {}, const Order& order = {});

// =========================================================================================================================================

/// Selects all data about asset in AssetItemEx
/// @param elementId asset element id
/// @return AssetItemEx or error
[[nodiscard]] Expected<AssetItemExt> itemExt(uint32_t elementId);

/// Selects all data about asset in AssetItemEx
/// @param conn database established connection
/// @param elementId asset element id
/// @return AssetItemEx or error
[[nodiscard]] Expected<AssetItemExt> itemExt(Connection& conn, uint32_t elementId);

// =========================================================================================================================================

/// Selects all data about asset in AssetItemEx
/// @param elementId asset element id
/// @param asset AssetItemEx to select to
/// @return nothing or error
[[nodiscard]] Expected<void> itemExt(uint32_t elementId, AssetItemExt& asset);

/// Selects all data about asset in AssetItemEx
/// @param conn database established connection
/// @param elementId asset element id
/// @param asset AssetItemEx to select to
/// @return nothing or error
[[nodiscard]] Expected<void> itemExt(Connection& conn, uint32_t elementId, AssetItemExt& asset);

// =========================================================================================================================================

/// Selects all data about asset by name
/// @param name asset element name
/// @return Element or error
[[nodiscard]] Expected<AssetItemExt> itemExt(const std::string& name);

/// Selects all data about asset by name
/// @param conn database established connection
/// @param name asset element name
/// @return Element or error
[[nodiscard]] Expected<AssetItemExt> itemExt(Connection& conn, const std::string& name);

// =========================================================================================================================================

/// Selects assets from given container
/// @param containerId asset element id
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<void> itemsByContainer(uint32_t containerId, Callback&& cb, const Filter& flt = {}, const Order& order = {});

/// Selects assets from given container
/// @param conn database established connection
/// @param containerId asset element id
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<void> itemsByContainer(
    Connection& conn, uint32_t containerId, Callback&& cb, const Filter& flt = {}, const Order& order = {});

// =========================================================================================================================================

/// Selects assets from given container
/// @param containerId asset element id
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<std::vector<AssetItemExt>> itemsByContainer(Callback&& cb, const Filter& flt = {}, const Order& order = {});

/// Selects assets from given container
/// @param conn database established connection
/// @param containerId asset element id
/// @param flt filter
/// @param order sort order
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<std::vector<AssetItemExt>> itemsByContainer(
    Connection& conn, uint32_t containerId, const Filter& flt = {}, const Order& order = {});

// =========================================================================================================================================

/// Select all assets in all (or without) containers
/// @param flt filter
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<void> itemsWithoutContainer(Callback&& cb, const Filter& filter = {}, const Order& order = {});

/// Select all assets in all (or without) containers
/// @param conn database established connection
/// @param flt filter
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<void> itemsWithoutContainer(Connection& conn, Callback&& cb, const Filter& filter = {}, const Order& order = {});

// =========================================================================================================================================

/// Select all assets in all (or without) containers
/// @param flt filter
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<std::vector<AssetItemExt>> itemsWithoutContainer(const Filter& filter = {}, const Order& order = {});

/// Select all assets in all (or without) containers
/// @param conn database established connection
/// @param flt filter
/// @param cb callback function
/// @return nothing or error
[[nodiscard]] Expected<std::vector<AssetItemExt>> itemsWithoutContainer(
    Connection& conn, const Filter& filter = {}, const Order& order = {});

// =========================================================================================================================================

/// Selects all ext_attributes of asset
/// @param elementId asset element id
/// @return Attributes map or error
[[nodiscard]] Expected<Attributes> extAttributes(uint32_t elementId);

/// Selects all ext_attributes of asset
/// @param conn database established connection
/// @param elementId asset element id
/// @return Attributes map or error
[[nodiscard]] Expected<Attributes> extAttributes(Connection& conn, uint32_t elementId);

// =========================================================================================================================================

/// Gets data about the links the specified device belongs to
/// @param elementId element id
/// @param linkTypeId link type id
/// @return list of links or error
[[nodiscard]] Expected<std::vector<AssetLink>> deviceLinksTo(uint32_t elementId, uint8_t linkTypeId = 1);

/// Gets data about the links the specified device belongs to
/// @param conn database established connection
/// @param elementId element id
/// @param linkTypeId link type id
/// @return list of links or error
[[nodiscard]] Expected<std::vector<AssetLink>> deviceLinksTo(Connection& conn, uint32_t elementId, uint8_t linkTypeId = 1);

} // namespace fty::db::asset::select
