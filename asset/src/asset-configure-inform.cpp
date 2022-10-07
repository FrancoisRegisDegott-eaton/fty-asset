/*
 *
 * Copyright (C) 2014 - 2018 Eaton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "asset/asset-configure-inform.h"
#include <fty_common_db_connection.h>
#include <fty_common.h>
#include <fty_common_db.h>
#include <fty_common_mlm_utils.h>
#include <fty_proto.h>
#include <thread>
#include <malamute.h>
#include <thread>
#include <fty_log.h>

namespace fty::asset {

static void* voidify(const std::string& str)
{
    return reinterpret_cast<void*>(const_cast<char*>(str.c_str()));
}

static zhash_t* s_map2zhash(const std::map<std::string, std::string>& m)
{
    zhash_t* ret = zhash_new();
    zhash_autofree(ret);
    for (const auto& it : m) {
        zhash_insert(ret, it.first.c_str(), voidify(it.second));
    }
    return ret;
}

// IMPORTANT NOTE: upses *must* be autofree
static bool getDcUPSes(fty::db::Connection& conn, const std::string& assetName, zhash_t* upses)
{
    if (!upses)
        return false;

    std::vector<std::string> listUps;

    auto cb = [&listUps](const fty::db::Row& row) {
        listUps.push_back(row.get("name"));
    };

    auto dcId = db::nameToAssetId(assetName);
    if (!dcId) {
        return false;
    }

    auto rv = db::selectAssetsByContainer(
        conn, *dcId, {persist::asset_type::DEVICE}, {persist::asset_subtype::UPS}, "", "active", cb);

    if (!rv) {
        return false;
    }

    int i = 0;
    for (const auto& ups : listUps) {
        char key[16];
        snprintf(key, sizeof(key), "ups%d", i);
        zhash_insert(upses, key, voidify(ups));
        i++;
    }
    return true;
}

Expected<void> sendConfigure(
    const std::vector<std::pair<db::AssetElement, persist::asset_operation>>& rows, const std::string& agentName)
{
    mlm_client_t* client = mlm_client_new();

    if (!client) {
        return unexpected("mlm_client_new () failed.");
    }

    int r = mlm_client_connect(client, MLM_ENDPOINT, 1000, agentName.c_str());
    if (r == -1) {
        mlm_client_destroy(&client);
        return unexpected("mlm_client_connect () failed.");
    }

    r = mlm_client_set_producer(client, FTY_PROTO_STREAM_ASSETS);
    if (r == -1) {
        mlm_client_destroy(&client);
        return unexpected(" mlm_client_set_producer () failed.");
    }

    fty::db::Connection conn;
    for (const auto& oneRow : rows) {

        std::string s_priority    = std::to_string(oneRow.first.priority);
        std::string s_parent      = std::to_string(oneRow.first.parentId);
        std::string s_asset_name  = oneRow.first.name;
        std::string s_asset_type  = persist::typeid_to_type(oneRow.first.typeId);
        std::string s_subtypeName = persist::subtypeid_to_subtype(oneRow.first.subtypeId);

        std::string subject;
        subject = persist::typeid_to_type(oneRow.first.typeId);
        subject.append(".");
        subject.append(persist::subtypeid_to_subtype(oneRow.first.subtypeId));
        subject.append("@");
        subject.append(oneRow.first.name);

        zhash_t* aux = zhash_new();
        zhash_autofree(aux);
        zhash_insert(aux, "priority", voidify(s_priority));
        zhash_insert(aux, "type", voidify(s_asset_type));
        zhash_insert(aux, "subtype", voidify(s_subtypeName));
        zhash_insert(aux, "parent", voidify(s_parent));
        zhash_insert(aux, "status", voidify(oneRow.first.status));

        // this is a bit hack, but we now that our topology ends with datacenter (hopefully)
        std::string dc_name;

        auto cb = [aux, &dc_name](const fty::db::Row& row) {
            static const std::vector<std::string> names({"parent_name1", "parent_name2", "parent_name3",
                "parent_name4", "parent_name5", "parent_name6",
                "parent_name7", "parent_name8", "parent_name9", "parent_name10"});

            for (const auto& name : names) {
                std::string foo = row.get(name);
                if (foo.empty())
                    continue;
                std::string hash_name{name};
                // 11 == strlen ("parent_name")
                hash_name.insert(11, 1, '.');
                zhash_insert(aux, hash_name.c_str(), voidify(foo));
                dc_name = foo;
            }
        };

        auto res = db::selectAssetElementSuperParent(oneRow.first.id, cb);
        if (!res) {
            logError("selectAssetElementSuperParent error: {}", res.error());
            zhash_destroy(&aux);
            mlm_client_destroy(&client);
            return unexpected("persist::select_asset_element_super_parent () failed.");
        }

        zhash_t* ext = s_map2zhash(oneRow.first.ext);

        zmsg_t* msg = fty_proto_encode_asset(aux, oneRow.first.name.c_str(), operation2str(oneRow.second).c_str(), ext);

        zhash_destroy(&ext); //useless
        zhash_destroy(&aux);

        r = mlm_client_send(client, subject.c_str(), &msg);
        zmsg_destroy(&msg);
        if (r != 0) {
            mlm_client_destroy(&client);
            return unexpected("mlm_client_send () failed.");
        }

        // ask fty-asset to republish so we would get UUID
        if (streq(operation2str(oneRow.second).c_str(), FTY_PROTO_ASSET_OP_CREATE) ||
            streq(operation2str(oneRow.second).c_str(), FTY_PROTO_ASSET_OP_UPDATE))
        {
            zmsg_t* republish = zmsg_new();
            zmsg_addstr(republish, s_asset_name.c_str());
            r = mlm_client_sendto(client, AGENT_FTY_ASSET, "REPUBLISH", nullptr, 5000, &republish);
            zmsg_destroy(&republish);
            if (r != 0) {
                log_error("sendto %s REPUBLISH %s failed.", AGENT_FTY_ASSET, s_asset_name.c_str());
            }
            //no response expected
        }

        // data for uptime
        if (oneRow.first.subtypeId == persist::asset_subtype::UPS) {
            zhash_t* aux1 = zhash_new();
            zhash_autofree(aux1);
            if (!getDcUPSes(conn, dc_name, aux1)) {
                log_error("Cannot read upses for dc with id = %s", dc_name.c_str());
            }

            zhash_update(aux1, "type", reinterpret_cast<void*>(const_cast<char*>("datacenter")));

            zmsg_t* msg1 = fty_proto_encode_asset(aux1, dc_name.c_str(), "inventory", nullptr);
            zhash_destroy(&aux1);

            std::string subject1 = "datacenter.unknown@" + dc_name;
            r = mlm_client_send(client, subject1.c_str(), &msg1);
            zmsg_destroy(&msg1);

            if (r != 0) {
                mlm_client_destroy(&client);
                return unexpected("mlm_client_send () failed.");
            }
        }
    }

    zclock_sleep(500); // ensure that everything was send before we destroy the client
    mlm_client_destroy(&client);

    return {};
}

Expected<void> sendConfigure(
    const db::AssetElement& row, persist::asset_operation actionType, const std::string& agentName)
{
    return sendConfigure({std::make_pair(row, actionType)}, agentName);
}

std::string generateMlmClientId(const std::string& client_name)
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string pid = ss.str();

    if (pid.empty()) {
        return client_name + "." + std::to_string(random());
    } else {
        return client_name + "." + pid;
    }
}

} // namespace fty::asset
