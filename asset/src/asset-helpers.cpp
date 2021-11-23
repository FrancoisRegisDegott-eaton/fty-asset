#include "asset/asset-helpers.h"
#include "asset/asset-db.h"
#include <ctime>
#include <fty_asset_dto.h>
#include <fty_common_agents.h>
#include <fty_common_db_connection.h>
#include <fty_common_mlm.h>
#include <regex>

#define AGENT_ASSET_ACTIVATOR      "etn-licensing-credits"
#define COMMAND_IS_ASSET_ACTIVABLE "GET_IS_ASSET_ACTIVABLE"
#define COMMAND_ACTIVATE_ASSET     "ACTIVATE_ASSET"
#define COMMAND_DEACTIVATE_ASSET   "DEACTIVATE_ASSET"

namespace fty::asset {

AssetExpected<uint32_t> checkElementIdentifier(const std::string& paramName, const std::string& paramValue)
{
    assert(!paramName.empty());
    if (paramValue.empty()) {
        return unexpected(error(Errors::ParamRequired).format(paramName));
    }

    static std::string prohibited = "_@%;\"";

    for (auto a : prohibited) {
        if (paramValue.find(a) != std::string::npos) {
            std::string err      = "value '{}' contains prohibited characters ({})"_tr.format(paramValue, prohibited);
            std::string expected = "valid identifier"_tr;
            return unexpected(error(Errors::BadParams).format(paramName, err, expected));
        }
    }

    if (auto eid = db::nameToAssetId(paramValue)) {
        return *eid;
    } else {
        std::string err      = "value '{}' is not valid identifier. Error: {}"_tr.format(paramValue, eid.error());
        std::string expected = "existing identifier"_tr;
        return unexpected(error(Errors::BadParams).format(paramName, err, expected));
    }
}

AssetExpected<std::string> sanitizeDate(const std::string& inp)
{
    static std::vector<std::string> formats = {"%d-%m-%Y", "%Y-%m-%d", "%d-%b-%y", "%d.%m.%Y", "%d %m %Y", "%m/%d/%Y"};

    struct tm timeinfo;
    for (const auto& fmt : formats) {
        if (!strptime(inp.c_str(), fmt.c_str(), &timeinfo)) {
            continue;
        }
        std::array<char, 11> buff;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
        std::strftime(buff.data(), buff.size(), fmt.c_str(), &timeinfo);
#pragma GCC diagnostic pop
        return std::string(buff.begin(), buff.end());
    }

    return unexpected("Not is ISO date"_tr);
}

AssetExpected<double> sanitizeValueDouble(const std::string& key, const std::string& value)
{
    try {
        std::size_t pos     = 0;
        double      d_value = std::stod(value, &pos);
        if (pos != value.length()) {
            return unexpected(error(Errors::BadParams).format(key, value, "value should be a number"_tr));
        }
        return d_value;
    } catch (const std::exception&) {
        return unexpected(error(Errors::BadParams).format(key, value, "value should be a number"_tr));
    }
}

AssetExpected<void> tryToPlaceAsset(uint32_t id, uint32_t parentId, uint32_t size, uint32_t loc)
{
    auto attr = db::selectExtAttributes(parentId);
    if (!attr) {
        return {};
    }

    if (!loc) {
        return unexpected("Position is wrong, should be greater than 0"_tr);
    }

    if (!size) {
        return unexpected("Size is wrong, should be greater than 0"_tr);
    }

    if (!attr->count("u_size")) {
        return unexpected("Size is not set"_tr);
    }

    std::vector<bool> place;
    place.resize(convert<size_t>(attr->at("u_size").value), false);

    auto children = db::selectAssetsByParent(parentId);
    if (!children) {
        return {};
    }

    for (const auto& child : *children) {
        if (child == id) {
            continue;
        }

        auto chAttr = db::selectExtAttributes(child);
        if (!chAttr) {
            continue;
        }
        if (!chAttr->count("u_size") && !chAttr->count("location_u_pos")) {
            continue;
        }

        size_t isize = convert<size_t>(chAttr->at("u_size").value);
        size_t iloc  = convert<size_t>(chAttr->at("location_u_pos").value) - 1;

        for (size_t i = iloc; i < iloc + isize; ++i) {
            if (i < place.size()) {
                place[i] = true;
            }
        }
    }

    for (size_t i = loc - 1; i < loc + size - 1; ++i) {
        if (i >= place.size()) {
            return unexpected("Asset is out bounds"_tr);
        }
        if (place[i]) {
            return unexpected("Asset place is occupied"_tr);
        }
    }

    return {};
}

static AssetExpected<std::vector<std::string>> activateRequest(const std::string& command, const std::string& asset)
{
    try {
        mlm::MlmSyncClient client(AGENT_FTY_ASSET, AGENT_ASSET_ACTIVATOR);

        logDebug("Sending {} request to {}", command, AGENT_ASSET_ACTIVATOR);

        std::vector<std::string> payload = {command, asset};

        std::vector<std::string> receivedFrames = client.syncRequestWithReply(payload);

        // check if the first frame we get is an error
        if (receivedFrames[0] == "ERROR") {
            if (receivedFrames.size() == 2) {
                return unexpected(receivedFrames.at(1));
            } else {
                return unexpected("Missing data for error");
            }
        }
        return receivedFrames;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

AssetExpected<bool> activation::isActivable(const std::string& asset)
{
    if (auto ret = activateRequest(COMMAND_IS_ASSET_ACTIVABLE, asset)) {
        logDebug("asset is activable = {}", ret->at(0));
        return fty::convert<bool>(ret->at(0));
    } else {
        return unexpected(ret.error());
    }
}

AssetExpected<bool> activation::isActivable(const FullAsset& asset)
{
    return isActivable(asset.toJson());
}

AssetExpected<void> activation::activate(const std::string& asset)
{
    if (auto ret = activateRequest(COMMAND_ACTIVATE_ASSET, asset); !ret) {
        return unexpected(ret.error());
    }
    return {};
}

AssetExpected<void> activation::activate(const FullAsset& asset)
{
    return activate(asset.toJson());
}

AssetExpected<void> activation::deactivate(const std::string& asset)
{
    if (auto ret = activateRequest(COMMAND_DEACTIVATE_ASSET, asset); !ret) {
        return unexpected(ret.error());
    }
    return {};
}

AssetExpected<void> activation::deactivate(const FullAsset& asset)
{
    return deactivate(asset.toJson());
}

AssetExpected<std::string> normName(const std::string& origName, uint32_t maxLen, uint32_t assetId)
{
    if (origName.length() < maxLen) {
        return origName;
    }

    static std::regex  rex("^.*~(\\d+)$");
    static std::string sql = R"(
        SELECT value
        FROM   t_bios_asset_ext_attributes
        WHERE
            keytag = 'name'
            AND (
                value = :name OR
                value LIKE :mask1 OR
                value LIKE :mask2
            )
            AND id_asset_element != : assetId
    )";

    std::string name = origName.substr(0, maxLen);

    try {
        fty::db::Connection conn;

        // clang-format off
        auto rows = conn.select(sql,
            "name"_p    = name,
            "mask1"_p   = name.substr(0, maxLen-2) + "~%",
            "mask2"_p   = name.substr(0, maxLen-3) + "~%",
            "assetId"_p = assetId
        );
        // clang-format on

        int         num = -1;
        std::smatch match;
        for (const auto& row : rows) {
            std::string val = row.get("value");
            if (std::regex_search(val, match, rex)) {
                int tnum = fty::convert<int>(match[1].str());
                if (tnum > num) {
                    num = tnum;
                }
            } else if (num == -1) {
                num = 0;
            }
        }

        if (num != -1) {
            std::string suffix = fty::convert<std::string>(num + 1);

            name = origName.substr(0, maxLen - 1 - suffix.length());
            name = fmt::format("{}~{}", name, suffix);
        }
    } catch (const std::exception& ex) {
        logError(ex.what());
        return fty::unexpected("Exception: {}", ex.what());
    }
    return name;
}

} // namespace fty::asset
