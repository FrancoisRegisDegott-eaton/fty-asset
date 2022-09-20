/*  =========================================================================
    fty_asset_server - Asset server, that takes care about distribution of asset information across the system

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

/*
@header
    fty_asset_server - Asset server, that takes care about distribution of asset information across the system
@discuss
     ASSET PROTOCOL

     ------------------------------------------------------------------------
     ## Topology request

     power topology request:
         subject: "TOPOLOGY"
         message: is a multipart message A/B
                 A = "TOPOLOGY_POWER" - mandatory
                 B = "asset_name" - mandatory

     power topology reply in "OK" case:
         subject: "TOPOLOGY"
         message: is a multipart message A/B/D/C1/.../CN
                 A = "TOPOLOGY_POWER" - mandatory
                 B = "asset_name" - mandatory
                 D = "OK" - mandatory
                 Ci = "asset_name" of power source - not mandatory
                     if there are no power devices
                      -> message is A/B/D

     power topology reply in "ERROR" case:
         subject: "TOPOLOGY"
         message: is a multipart message A/B/D/E
                 A = "TOPOLOGY_POWER" - mandatory
                 B = "asset_name" - mandatory
                 D = "ERROR" - mandatory
                 E = "ASSET_NOT_FOUND"/"INTERNAL_ERROR" - mandatory


    ------------------------------------------------------------------------
    ## Asset manipulation protocol

    REQ:
        subject: "ASSET_MANIPULATION"
        Message is a fty protocol (fty_proto_t) message

        *) read-only/fty_proto ASSET message

        where:
        * 'operation' is one of [ create | create-force | update | delete | retire ].
           Asset messages with different operation value are discarded and not replied to.
        * 'read-only' tells us whether ext attributes should be inserted as read-only or not.
           Allowed values are READONLY and READWRITE.

    REP:
        subject: same as in REQ
        Message is a multipart string message:

        * OK/<asset_id>
        * ERROR/<reason>

        where:
 (OPTIONAL)     <asset_id>  = asset id (in case of create, update operation)
                <reason>    = Error message/code TODO

    Note: in REQ message certain asset information are encoded as follows

      'ext' field
          Power Links - key: "power_link.<device_name>", value: "<first_outlet_num>/<second_outlet_num>", i.e.
1 --> 2 == "1/2" Groups - key: "group", value: "<group_name_1>/.../<group_name_N>"


    ------------------------------------------------------------------------
    ## ASSETS in container

    REQ:
        subject: "ASSETS_IN_CONTAINER"
        Message is a multipart string message

        * GET/<container name>/<type 1>/.../<type n>

        where:
            <container name>        = Name of the container things belongs to that
                                      when empty, no container is used, so all assets are take in account
            <type X>                = Type or subtype to be returned. Possible values are
                                      ups
                                      TODO: add more
                                      when empty, no filtering is done
    REP:
        subject: "ASSETS_IN_CONTAINER"
        Message is a multipart message:

        * OK                         = empty container
        * OK/<asset 1>/.../<asset N> = non-empty
        * ERROR/<reason>

        where:
            <reason>          = ASSET_NOT_FOUND / INTERNAL_ERROR / BAD_COMMAND

    REQ:
        subject: "ASSETS"
        Message is a multipart string message

        * GET/<uuid>/<type 1>/.../<type n>

        where:
            <uuid>                  = zuuid of  message
            <type X>                = Type or subtype to be returned. Possible values are
                                      ups
                                      TODO: add more
                                      when empty, no filtering is done
    REP:
        subject: "ASSETS"
        Message is a multipart message:

        * OK                         = empty container
        * OK/<uuid>/<asset 1>/.../<asset N> = non-empty
        * ERROR/<uuid>/<reason>

        where:
            <reason>          = ASSET_NOT_FOUND / INTERNAL_ERROR / BAD_COMMAND
    ------------------------------------------------------------------------
    ## REPUBLISH

    REQ:
        subject: "REPUBLISH"
        Message is a multipart string message

        /asset1/asset2/asset3       - republish asset information about asset1 asset2 and asset3
        /$all                       - republish information about all assets

     ------------------------------------------------------------------------
     ## ENAME_FROM_INAME

     request user-friendly name for given iname:
         subject: "ENAME_FROM_INAME"
         message: is a string message A
                 B = "asset_iname" - mandatory

     reply in "OK" case:
         subject: "ENAME_FROM_INAME"
         message: is a multipart message A/B
                 A = "OK" - mandatory
                 B = user-friendly name of given asset - mandatory

     reply in "ERROR" case:
         subject: "ENAME_FROM_INAME"
         message: is a multipart message A/B
                 A = "ERROR" - mandatory
                 B = "ASSET_NOT_FOUND"/"MISSING_INAME" - mandatory


     ------------------------------------------------------------------------
     ## ASSET_DETAIL

     request all the available data about given asset:
         subject: "ASSET_DETAIL"
         message: is a multipart message A/B/C
                 A = "GET" - mandatory
                 B = uuid - mandatory
                 C = "asset_name" - mandatory

     power topology reply in "OK" case:
         subject: "ASSET_DETAIL"
         message: is fty-proto asset update message

     power topology reply in "ERROR" case:
         subject: "ASSET_DETAIL"
         message: is a multipart message A/B
                 A = "ERROR" - mandatory
                 B = "BAD_COMMAND"/"INTERNAL_ERROR"/"ASSET_NOT_FOUND" - mandatory


@end
*/

#include "fty_asset_server.h"
#include "fty_asset_autoupdate.h"

#include "test_str.h"
#include "asset-server.h"
#include "asset/asset-utils.h"
#include "asset/dbhelpers.h"

#include "total_power.h"
#include "topology_processor.h"
#include "topology_power.h"

#include <ctime>
#include <string>
#include <iostream>

#include <fty_asset_dto.h>
#include <fty_common.h>
#include <fty_common_db_uptime.h>
#include <fty_common_messagebus.h>
#include <functional>
#include <malamute.h>
#include <mlm_client.h>
#include <sys/time.h>
#include <tntdb/connect.h>
#include <fty_proto.h>
#include <fty_common.h>
#include <fty_common_db.h>
#include <fty_common_mlm.h>
#include <uuid/uuid.h>

#include <cassert>

bool g_testMode = false;

// =============================================================================
// TOPOLOGY/POWER command processing (completed reply)
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> POWER <assetID>
// =============================================================================

static void s_process_TopologyPower(
    const std::string& client_name, const char* asset_name, bool testMode, zmsg_t* reply)
{
    assert (reply);

    log_debug("%s:\tTOPOLOGY POWER asset_name: %s", client_name.c_str(), asset_name);

    // result of power topology - list of power device names
    std::vector<std::string> powerDevices{};
    std::string              assetName(asset_name ? asset_name : "");

    // select power devices
    int r = select_devices_total_power(assetName, powerDevices, testMode);

    zmsg_addstr(reply, assetName.c_str());

    // form a message according the contract for the case "OK" and for the case "ERROR"
    if (r == -1) {
        log_error("%s:\tTOPOLOGY POWER: Cannot select power sources (%s)", client_name.c_str(), asset_name);

        zmsg_addstr(reply, "ERROR");
        // zmsg_addstr (reply, "INTERNAL_ERROR");
        zmsg_addstr(reply, TRANSLATE_ME("Internal error").c_str());
    } else if (r == -2) {
        log_error("%s:\tTOPOLOGY POWER: Asset was not found (%s)", client_name.c_str(), asset_name);

        zmsg_addstr(reply, "ERROR");
        // zmsg_addstr (reply, "ASSET_NOT_FOUND");
        zmsg_addstr(reply, TRANSLATE_ME("Asset not found").c_str());
    } else {
        log_debug("%s:\tPower topology for '%s':", client_name.c_str(), asset_name);

        zmsg_addstr(reply, "OK");
        for (const auto& powerDeviceName : powerDevices) {
            log_debug("%s:\t\t%s", client_name.c_str(), powerDeviceName.c_str());
            zmsg_addstr(reply, powerDeviceName.c_str());
        }
    }
}

// =============================================================================
// TOPOLOGY/POWER_TO command processing (completed reply)
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> POWER_TO <assetID>
// =============================================================================

static void s_process_TopologyPowerTo(const std::string& client_name, const char* asset_name, zmsg_t* reply)
{
    assert (reply);

    log_debug("%s:\tTOPOLOGY POWER_TO asset_name: %s", client_name.c_str(), asset_name);

    std::string assetName(asset_name ? asset_name : "");
    std::string result;    // JSON payload
    std::string errReason; // JSON payload (TRANSLATE_ME)
    int         r = topology_power_to(assetName, result, errReason);

    zmsg_addstr(reply, assetName.c_str());

    if (r != 0) {
        log_error("%s:\tTOPOLOGY POWER_TO r: %d (asset_name: %s)", client_name.c_str(), r, asset_name);

        if (errReason.empty()) {
            if (!asset_name)
                errReason = TRANSLATE_ME("Missing argument");
            else
                errReason = TRANSLATE_ME("Internal error");
        }
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, errReason.c_str());
    } else {
        zmsg_addstr(reply, "OK");
        zmsg_addstr(reply, result.c_str()); // JSON in one frame
    }
}

// =============================================================================
// TOPOLOGY/POWERCHAINS command processing (completed reply)
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> POWERCHAINS <select_cmd> <assetID>
// <select_cmd> in {"to", "from", "filter_dc", "filter_group"}
// =============================================================================

static void s_process_TopologyPowerchains(
    const std::string& client_name, const char* select_cmd, const char* asset_name, zmsg_t* reply)
{
    assert (reply);

    log_debug("%s:\tTOPOLOGY POWERCHAINS select_cmd: %s, asset_name: %s", client_name.c_str(), select_cmd,
        asset_name);

    std::string command(select_cmd ? select_cmd : "");
    std::string assetName(asset_name ? asset_name : "");
    std::string result;    // JSON payload
    std::string errReason; // JSON payload (TRANSLATE_ME)
    int         r = topology_power_process(command, assetName, result, errReason);

    zmsg_addstr(reply, assetName.c_str());

    if (r != 0) {
        log_error("%s:\tTOPOLOGY POWERCHAINS r: %d (cmd: %s, asset_name: %s)", client_name.c_str(), r,
            select_cmd, asset_name);

        if (errReason.empty()) {
            if (!asset_name)
                errReason = TRANSLATE_ME("Missing argument");
            else
                errReason = TRANSLATE_ME("Internal error");
        }
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, errReason.c_str());
    } else {
        zmsg_addstr(reply, "OK");
        zmsg_addstr(reply, result.c_str()); // JSON in one frame
    }
}

// =============================================================================
// TOPOLOGY/LOCATION command processing (completed reply)
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> LOCATION <select_cmd> <assetID> <options>
// <select_cmd> in {"to", "from"}
// see topology_location_process() for allowed options
// =============================================================================

static void s_process_TopologyLocation(const std::string& client_name, const char* select_cmd,
    const char* asset_name, const char* cmd_options, zmsg_t* reply)
{
    assert (reply);

    log_debug("%s:\tTOPOLOGY LOCATION select_cmd: %s, asset_name: %s (options: %s)", client_name.c_str(),
        select_cmd, asset_name, cmd_options);

    std::string command(select_cmd ? select_cmd : "");
    std::string assetName(asset_name ? asset_name : "");
    std::string options(cmd_options ? cmd_options : "");
    std::string result;    // JSON payload
    std::string errReason; // JSON payload (TRANSLATE_ME)
    int         r = topology_location_process(command, assetName, options, result, errReason);

    zmsg_addstr(reply, assetName.c_str());

    if (r != 0) {
        log_error("%s:\tTOPOLOGY LOCATION r: %d (cmd: %s, asset_name: %s, options: %s)", client_name.c_str(),
            r, select_cmd, asset_name, cmd_options);

        if (errReason.empty()) {
            if (!asset_name)
                errReason = TRANSLATE_ME("Missing argument");
            else
                errReason = TRANSLATE_ME("Internal error");
        }
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, errReason.c_str());
    } else {
        zmsg_addstr(reply, "OK");
        zmsg_addstr(reply, result.c_str()); // JSON in one frame
    }
}

// =============================================================================
// TOPOLOGY/INPUT_POWERCHAIN command processing (completed reply)
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> INPUT_POWERCHAIN <assetID>
// <assetID> shall be a datacenter
// =============================================================================

static void s_process_TopologyInputPowerchain(
    const std::string& client_name, const char* asset_name, zmsg_t* reply)
{
    assert (reply);

    log_debug("%s:\tTOPOLOGY INPUT_POWERCHAIN asset_name: %s", client_name.c_str(), asset_name);

    std::string assetName(asset_name ? asset_name : "");
    std::string result;    // JSON payload
    std::string errReason; // JSON payload (TRANSLATE_ME)
    int         r = topology_input_powerchain_process(assetName, result, errReason);

    zmsg_addstr(reply, assetName.c_str());

    if (r != 0) {
        log_error(
            "%s:\tTOPOLOGY INPUT_POWERCHAIN r: %d (asset_name: %s)", client_name.c_str(), r, asset_name);

        if (errReason.empty()) {
            if (!asset_name)
                errReason = TRANSLATE_ME("Missing argument");
            else
                errReason = TRANSLATE_ME("Internal error");
        }
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, errReason.c_str());
    } else {
        zmsg_addstr(reply, "OK");
        zmsg_addstr(reply, result.c_str()); // JSON in one frame
    }
}

// =============================================================================
//         Functionality for TOPOLOGY processing
// =============================================================================
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> POWER <assetID>
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> POWER_TO <assetID>
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> POWERCHAINS <select_cmd> <assetID>
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> LOCATION <select_cmd> <assetID> <options>
// bmsg request asset-agent TOPOLOGY REQUEST <uuid> INPUT_POWERCHAIN <assetID>
// =============================================================================

static void s_handle_subject_topology(const fty::AssetServer& server, zmsg_t* msg)
{
    assert (msg);

    char*   message_type = zmsg_popstr(msg);
    char*   uuid         = zmsg_popstr(msg);
    char*   command      = zmsg_popstr(msg);
    zmsg_t* reply        = zmsg_new();

    const std::string& client_name = server.getAgentName();

    log_debug(
        "%s:\tmessage_type: %s, uuid: %s, command: %s", client_name.c_str(), message_type, uuid, command);

    if (!message_type) {
        log_error("%s:\tExpected message_type for subject=TOPOLOGY", client_name.c_str());
    } else if (!uuid) {
        log_error("%s:\tExpected uuid for subject=TOPOLOGY", client_name.c_str());
    } else if (!command) {
        log_error("%s:\tExpected command for subject=TOPOLOGY", client_name.c_str());
    } else if (!reply) {
        log_error("%s:\tTOPOLOGY %s: reply allocation failed", client_name.c_str(), command);
    } else {
        // message model always enforce reply
        zmsg_addstr(reply, uuid);
        zmsg_addstr(reply, "REPLY");
        zmsg_addstr(reply, command);

        if (!streq(message_type, "REQUEST")) {
            log_error("%s:\tExpected REQUEST message_type for subject=TOPOLOGY (message_type: %s)",
                client_name.c_str(), message_type);
            zmsg_addstr(reply, "ERROR"); // status
            // reason, JSON payload (TRANSLATE_ME)
            zmsg_addstr(reply, TRANSLATE_ME("REQUEST_MSGTYPE_EXPECTED (msg type: %s)", message_type).c_str());
        } else if (streq(command, "POWER")) {
            char* asset_name = zmsg_popstr(msg);
            s_process_TopologyPower(server.getAgentName(), asset_name, server.getTestMode(), reply);
            zstr_free(&asset_name);
        } else if (streq(command, "POWER_TO")) {
            char* asset_name = zmsg_popstr(msg);
            s_process_TopologyPowerTo(server.getAgentName(), asset_name, reply);
            zstr_free(&asset_name);
        } else if (streq(command, "POWERCHAINS")) {
            char* select_cmd = zmsg_popstr(msg);
            char* asset_name = zmsg_popstr(msg);
            s_process_TopologyPowerchains(server.getAgentName(), select_cmd, asset_name, reply);
            zstr_free(&asset_name);
            zstr_free(&select_cmd);
        } else if (streq(command, "LOCATION")) {
            char* select_cmd = zmsg_popstr(msg);
            char* asset_name = zmsg_popstr(msg);
            char* options    = zmsg_popstr(msg); // can be NULL
            s_process_TopologyLocation(server.getAgentName(), select_cmd, asset_name, options, reply);
            zstr_free(&options);
            zstr_free(&asset_name);
            zstr_free(&select_cmd);
        } else if (streq(command, "INPUT_POWERCHAIN")) {
            char* asset_name = zmsg_popstr(msg);
            s_process_TopologyInputPowerchain(server.getAgentName(), asset_name, reply);
            zstr_free(&asset_name);
        } else {
            log_error("%s:\tUnexpected command for subject=TOPOLOGY (%s)", client_name.c_str(), command);
            zmsg_addstr(reply, "ERROR"); // status
            // reason, JSON payload (TRANSLATE_ME)
            zmsg_addstr(reply, TRANSLATE_ME("UNEXPECTED_COMMAND (command: %s)", command).c_str());
        }

        // send reply
        int r = mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
            mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "TOPOLOGY", NULL, 5000,
            &reply);
        if (r != 0) {
            log_error("%s:\tTOPOLOGY (command: %s): mlm_client_sendto failed", client_name.c_str(), command);
        }
    }

    zmsg_destroy(&reply);
    zstr_free(&command);
    zstr_free(&uuid);
    zstr_free(&message_type);
}

static void s_handle_subject_assets_in_container(const fty::AssetServer& server, zmsg_t* msg)
{
    assert (msg);

    const std::string& client_name = server.getAgentName();

    if (zmsg_size(msg) < 2) {
        log_error("%s:\tASSETS_IN_CONTAINER: incoming message have less than 2 frames", client_name.c_str());
        return;
    }

    zmsg_t* reply = zmsg_new();

    {
        char* c_command = zmsg_popstr(msg);
        if (!streq(c_command, "GET")) {
            log_error("%s:\tASSETS_IN_CONTAINER: bad command '%s', expected GET", client_name.c_str(), c_command);
            zmsg_addstr(reply, "ERROR");
            zmsg_addstr(reply, "BAD_COMMAND");
        }
        zstr_free(&c_command);
    }

    // if there is no error, call SQL
    if (zmsg_size(reply) == 0) {
        std::string container_name;
        {
            char* c_container_name = zmsg_popstr(msg);
            container_name = c_container_name ? c_container_name : "";
            zstr_free(&c_container_name);
        }

        std::set<std::string> filters;
        while (zmsg_size(msg) > 0) {
            char* filter = zmsg_popstr(msg);
            filters.insert(filter);
            zstr_free(&filter);
        }

        std::vector<std::string> assets;
        int rv = select_assets_by_container(container_name, filters, assets, server.getTestMode());

        if (rv == 0) {
            zmsg_addstr(reply, "OK");
            for (const auto& dev : assets)
                zmsg_addstr(reply, dev.c_str());
        }
        else {
            zmsg_addstr(reply, "ERROR");
            zmsg_addstr(reply, ((rv == -2) ? "ASSET_NOT_FOUND" : "INTERNAL_ERROR"));
        }
    }

    // send the reply
    int rv = mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
        mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ASSETS_IN_CONTAINER", NULL,
        5000, &reply);

    if (rv == -1) {
        log_error("%s:\tASSETS_IN_CONTAINER: mlm_client_sendto failed", client_name.c_str());
    }

    zmsg_destroy(&reply);
}

static void s_handle_subject_ename_from_iname(const fty::AssetServer& server, zmsg_t* msg)
{
    assert (msg);

    const std::string& client_name = server.getAgentName();

    zmsg_t* reply = zmsg_new();

    if (zmsg_size(msg) < 1) {
        log_error("%s:\tENAME_FROM_INAME: incoming message have less than 1 frame", client_name.c_str());
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "MISSING_INAME");
    }
    else {
        char* iname_str = zmsg_popstr(msg);
        std::string iname(iname_str ? iname_str : "");
        zstr_free(&iname_str);

        std::string ename;
        select_ename_from_iname(iname, ename, server.getTestMode());

        if (ename.empty()) {
            zmsg_addstr(reply, "ERROR");
            zmsg_addstr(reply, "ASSET_NOT_FOUND");
        } else {
            zmsg_addstr(reply, "OK");
            zmsg_addstr(reply, ename.c_str());
        }
    }

    // send the reply
    int rv = mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
        mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ENAME_FROM_INAME", NULL,
        5000, &reply);

    if (rv == -1) {
        log_error("%s:\tENAME_FROM_INAME: mlm_client_sendto failed", client_name.c_str());
    }

    zmsg_destroy(&reply);
}

//TODO rework error processing
static void s_handle_subject_assets(const fty::AssetServer& server, zmsg_t* msg)
{
    assert (msg);

    const std::string& client_name = server.getAgentName();

    zmsg_t* reply = zmsg_new();
    if (zmsg_size(msg) < 1) {
        log_error("%s:\tASSETS: incoming message have less than 1 frame", client_name.c_str());
        zmsg_addstr(reply, "0");
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "MISSING_COMMAND");
        mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
            mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ASSETS", NULL, 5000,
            &reply);
        zmsg_destroy(&reply);
        return;
    }

    char* c_command = zmsg_popstr(msg);
    if (!streq(c_command, "GET")) {

        const char *sender = mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient()));
        const char *subject = mlm_client_subject(const_cast<mlm_client_t*>(server.getMailboxClient()));
        log_error("%s:\tASSETS command GET expected (command: %s, sender: %s, subject: %s",
            client_name.c_str(), c_command, sender, subject);

        char* uuid = zmsg_popstr(msg);
        if (uuid)
            zmsg_addstr(reply, uuid);
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "BAD_COMMAND");
        mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
            sender, "ASSETS", NULL, 5000, &reply);

        zstr_free(&c_command);
        zstr_free(&uuid);
        zmsg_destroy(&reply);
        return;
    }
    zstr_free(&c_command);

    char* uuid = zmsg_popstr(msg);

    std::set<std::string> filters;
    while (zmsg_size(msg) > 0) {
        char* filter = zmsg_popstr(msg);
        filters.insert(filter);
        zstr_free(&filter);
    }

    std::vector<std::string> assets;
    int                      rv = 0;

    // if there is no error msg prepared, call SQL
    if (zmsg_size(msg) == 0) {
        rv = select_assets_by_filter(filters, assets, server.getTestMode());
    }

    zmsg_addstr(reply, uuid); // reply, uuid common frame

    if (rv == -1) {
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "INTERNAL_ERROR");
    } else if (rv == -2) {
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "ASSET_NOT_FOUND");
    } else {
        zmsg_addstr(reply, "OK");
        for (const auto& dev : assets)
            zmsg_addstr(reply, dev.c_str());
    }

    // send the reply
    rv = mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
        mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ASSETS", NULL, 5000,
        &reply);

    if (rv == -1) {
        log_error("%s:\tASSETS: mlm_client_sendto failed", client_name.c_str());
    }

    zstr_free(&uuid);
    zmsg_destroy(&reply);
}

static zmsg_t* s_publish_create_or_update_asset_msg(const std::string& client_name,
    const std::string& asset_name, const char* operation, std::string& subject, bool test_mode,
    bool /*read_only*/)
{
    zhash_t* aux = zhash_new();
    zhash_t* ext = zhash_new();
    if (!(aux && ext)) {
        log_error("%s:\tMemory allocation failed", client_name.c_str());
        zhash_destroy(&aux);
        zhash_destroy(&ext);
        return NULL;
    }
    zhash_autofree(aux);
    zhash_autofree(ext);

    uint32_t asset_id = 0;

    std::function<void(const tntdb::Row&)> cb1 = [aux, &asset_id, asset_name](const tntdb::Row& row) {
        int foo_i = 0;
        row["priority"].get(foo_i);
        zhash_insert(aux, "priority", static_cast<void*>(const_cast<char*>(std::to_string(foo_i).c_str())));

        foo_i = 0;
        row["id_type"].get(foo_i);
        zhash_insert(aux, "type", static_cast<void*>(const_cast<char*>(persist::typeid_to_type(static_cast<uint16_t>(foo_i)).c_str())));

        // additional aux items (requiered by uptime)
        if (streq(persist::typeid_to_type(static_cast<uint16_t>(foo_i)).c_str(), "datacenter")) {
#if 0 // original 2.4.0 (memleaks, get_dc_upses() insert strdup'ed items)
            if (!DBUptime::get_dc_upses(asset_name.c_str(), aux)) {
                log_error("Cannot read upses for dc with id = %s", asset_name.c_str());
            }
#else // no memleaks (use a non autofree interm. zhash on get_dc_upses() call)
            zhash_t* upses = zhash_new();
            if (!DBUptime::get_dc_upses(asset_name.c_str(), upses)) {
                log_error("Cannot read upses for dc with id = %s", asset_name.c_str());
            }
            else {
                zlist_t *keys = zhash_keys(upses);
                for (void* it = zlist_first(keys); it; it = zlist_next(keys)) {
                    const char* key = static_cast<const char*>(it);
                    zhash_freefn(upses, key, free); // to free strdup'ed item
                    zhash_insert(aux, key, zhash_lookup(upses, key)); // aux insert (strdup)
                    //const char* value = static_cast<const char*>(zhash_lookup(upses, key));
                    //log_trace("after get_dc_upses(), aux insert (%s, %s)", key, value);
                }
                zlist_destroy(&keys);
            }
            zhash_destroy(&upses);
#endif
        }
        foo_i = 0;
        row["subtype_id"].get(foo_i);
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>(persist::subtypeid_to_subtype(static_cast<uint16_t>(foo_i)).c_str())));

        foo_i = 0;
        row["id_parent"].get(foo_i);
        zhash_insert(aux, "parent", static_cast<void*>( const_cast<char*>(std::to_string(foo_i).c_str())));

        std::string foo_s;
        row["status"].get(foo_s);
        zhash_insert(aux, "status", static_cast<void*>( const_cast<char*>(foo_s.c_str())));

        row["id"].get(asset_id);
    };

    // select basic info
    int rv = select_asset_element_basic(asset_name, cb1, test_mode);
    if (rv != 0) {
        log_warning("%s:\tCannot select info about '%s'", client_name.c_str(), asset_name.c_str());
        zhash_destroy(&aux);
        zhash_destroy(&ext);
        return NULL;
    }

    std::function<void(const tntdb::Row&)> cb2 = [ext](const tntdb::Row& row) {
        std::string keytag;
        row["keytag"].get(keytag);
        std::string value;
        row["value"].get(value);
        zhash_insert(ext, keytag.c_str(), static_cast<void*>( const_cast<char*>(value.c_str())));
    };

    // select ext attributes
    rv = select_ext_attributes(asset_id, cb2, test_mode);
    if (rv != 0) {
        log_warning("%s:\tCannot select ext attributes for '%s'", client_name.c_str(), asset_name.c_str());
        zhash_destroy(&aux);
        zhash_destroy(&ext);
        return NULL;
    }

    // handle required but missing ext. attributes (inventory)
    {
        zhash_t* inventory = zhash_new();
        zhash_autofree(inventory);

        // create uuid ext attribute if missing
        if (!zhash_lookup(ext, "uuid")) {
            const char* serial = static_cast<const char*>(zhash_lookup(ext, "serial_no"));
            const char* model  = static_cast<const char*>(zhash_lookup(ext, "model"));
            const char* mfr    = static_cast<const char*>(zhash_lookup(ext, "manufacturer"));

            fty_uuid_t* uuid = fty_uuid_new();
            const char* uuid_new = nullptr;
            if (serial && model && mfr) {
                // we have all information => create uuid
                uuid_new = fty_uuid_calculate(uuid, mfr, model, serial);
            }
            else {
                // generate random uuid
                uuid_new = fty_uuid_generate(uuid);
            }
            zhash_insert(inventory, "uuid", static_cast<void*>( const_cast<char*>(uuid_new)));
            fty_uuid_destroy(&uuid);
        }

        // create timestamp ext attribute if missing
        if (!zhash_lookup(ext, "create_ts")) {
            std::time_t timestamp = std::time(NULL);
            char        mbstr[100];
            std::strftime(mbstr, sizeof(mbstr), "%FT%T%z", std::localtime(&timestamp));
            zhash_insert(inventory, "create_ts", static_cast<void*>( const_cast<char*>(mbstr)));
        }

        if (zhash_size(inventory) != 0) {
            // update ext
            for (void* it = zhash_first(inventory); it != NULL; it = zhash_next(inventory)) {
                auto keytag = zhash_cursor(inventory);
                auto value = it;
                zhash_insert(ext, keytag, value);
            }
            // update db inventory
            process_insert_inventory(asset_name.c_str(), inventory, true, test_mode);
        }

        zhash_destroy(&inventory);
    }

    std::function<void(const tntdb::Row&)> cb3 = [aux](const tntdb::Row& row) {
        static const std::vector<std::string> parentNames({"parent_name1", "parent_name2", "parent_name3",
                "parent_name4", "parent_name5", "parent_name6",
                "parent_name7", "parent_name8", "parent_name9", "parent_name10"});
        for (const auto& name : parentNames) {
            std::string foo;
            row[name].get(foo);
            if (foo.empty())
                continue;
            std::string hash_name = name;
            // 11 == strlen ("parent_name")
            hash_name.insert(11, 1, '.'); // parent_nameX -> parent_name.X
            zhash_insert(aux, hash_name.c_str(), static_cast<void*>( const_cast<char*>(foo.c_str())));
        }
    };

    // select "physical topology"
    rv = select_asset_element_super_parent(asset_id, cb3, test_mode);
    if (rv != 0) {
        zhash_destroy(&aux);
        zhash_destroy(&ext);
        log_error(
            "%s:\tselect_asset_element_super_parent ('%s') failed.", client_name.c_str(), asset_name.c_str());
        return NULL;
    }

    // other information like, groups, powerchain for now are not included in the message
    const char* type = static_cast<const char*>(zhash_lookup(aux, "type"));
    const char* subtype = static_cast<const char*>(zhash_lookup(aux, "subtype"));

    subject = (type == NULL) ? "unknown" : type;
    subject.append(".");
    subject.append((subtype == NULL) ? "unknown" : subtype);
    subject.append("@");
    subject.append(asset_name);

    log_debug("notifying ASSETS %s %s ..", operation, subject.c_str());

    zmsg_t* msg = fty_proto_encode_asset(aux, asset_name.c_str(), operation, ext);

    zhash_destroy(&ext);
    zhash_destroy(&aux);

    return msg;
}

void send_create_or_update_asset(
    const fty::AssetServer& server, const std::string& asset_name, const char* operation, bool read_only)
{
    std::string subject;
    auto        msg = s_publish_create_or_update_asset_msg(
        server.getAgentName(), asset_name, operation, subject, server.getTestMode(), read_only);
    if (NULL == msg ||
        0 != mlm_client_send(const_cast<mlm_client_t*>(server.getStreamClient()), subject.c_str(), &msg)) {
        log_info("%s:\tmlm_client_send not sending message for asset '%s'", server.getAgentName().c_str(),
            asset_name.c_str());
    }
    zmsg_destroy(&msg);
}

static void s_sendto_create_or_update_asset(const fty::AssetServer& server, const std::string& asset_name,
    const char* operation, const char* address, const char* uuid)
{
    std::string subject;
    auto        msg = s_publish_create_or_update_asset_msg(
        server.getAgentName(), asset_name, operation, subject, server.getTestMode(), false);
    if (NULL == msg) {
        msg = zmsg_new();
        log_error("%s:\tASSET_DETAIL: asset not found", server.getAgentName().c_str());
        zmsg_addstr(msg, "ERROR");
        zmsg_addstr(msg, "ASSET_NOT_FOUND");
    }
    zmsg_pushstr(msg, uuid);
    int rv = mlm_client_sendto(
        const_cast<mlm_client_t*>(server.getMailboxClient()), address, subject.c_str(), NULL, 5000, &msg);
    if (rv != 0) {
        log_error(
            "%s:\tmlm_client_send failed for asset '%s'", server.getAgentName().c_str(), asset_name.c_str());
    }
}

static void s_handle_subject_asset_detail(const fty::AssetServer& server, zmsg_t** zmessage_p)
{
    if (!zmessage_p || !*zmessage_p)
        return;
    zmsg_t* zmessage = *zmessage_p;

    char* c_command = zmsg_popstr(zmessage);
    if (!streq(c_command, "GET")) {
        char*   uuid  = zmsg_popstr(zmessage);
        zmsg_t* reply = zmsg_new();
        log_error(
            "%s:\tASSET_DETAIL: bad command '%s', expected GET", server.getAgentName().c_str(), c_command);
        if (uuid)
            zmsg_addstr(reply, uuid);
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "BAD_COMMAND");
        mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
            mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ASSET_DETAIL", NULL,
            5000, &reply);
        zstr_free(&uuid);
        zstr_free(&c_command);
        zmsg_destroy(&reply);
        return;
    }
    zstr_free(&c_command);

    // select an asset and publish it through mailbox
    char* uuid       = zmsg_popstr(zmessage);
    char* asset_name = zmsg_popstr(zmessage);
    s_sendto_create_or_update_asset(server, asset_name, FTY_PROTO_ASSET_OP_UPDATE,
        mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), uuid);
    zstr_free(&asset_name);
    zstr_free(&uuid);
}

static void s_handle_subject_asset_manipulation(const fty::AssetServer& server, zmsg_t** zmessage_p)
{
    const std::string& client_name = server.getAgentName();

    // Check request format
    if (!zmessage_p || !*zmessage_p)
        return;
    zmsg_t* zmessage = *zmessage_p;
    zmsg_t* reply    = zmsg_new();

    char* read_only_s = zmsg_popstr(zmessage);
    bool  read_only;
    if (read_only_s && streq(read_only_s, "READONLY")) {
        read_only = true;
    } else if (read_only_s && streq(read_only_s, "READWRITE")) {
        read_only = false;
    } else {
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, "BAD_COMMAND");
        mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
            mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ASSET_MANIPULATION",
            NULL, 5000, &reply);
        zstr_free(&read_only_s);
        zmsg_destroy(&reply);
        return;
    }
    zstr_free(&read_only_s);

    if (!fty_proto_is(zmessage)) {
        log_error("%s:\tASSET_MANIPULATION: receiver message is not fty_proto", client_name.c_str());
        zmsg_destroy(&reply);
        return;
    }

    fty_proto_t* proto = fty_proto_decode(zmessage_p);
    if (!proto) {
        log_error("%s:\tASSET_MANIPULATION: failed to decode message", client_name.c_str());
        zmsg_destroy(&reply);
        return;
    }

    fty_proto_print(proto);

    // get operation from message
    const char* operation = fty_proto_operation(proto);

    try {
        // asset manipulation is disabled
        if (server.getGlobalConfigurability() == 0) {
            throw std::runtime_error("Licensing limitation hit - asset manipulation is prohibited.");
        }

        // get asset from fty-proto
        fty::AssetImpl asset;
        fty::Asset::fromFtyProto(proto, asset, read_only, server.getTestMode());
        log_debug(
            "s_handle_subject_asset_manipulation(): Processing operation '%s' "
            "for asset named '%s' with type '%s' and subtype '%s'",
            operation, asset.getInternalName().c_str(), asset.getAssetType().c_str(),
            asset.getAssetSubtype().c_str());

        if (streq(operation, "create") || streq(operation, "create-force")) {
            bool requestActivation = (asset.getAssetStatus() == fty::AssetStatus::Active);
            // create-force -> tryActivate = true
            if (!asset.isActivable()) {
                if (streq(operation, "create-force")) {
                    asset.setAssetStatus(fty::AssetStatus::Nonactive);
                } else {
                    throw std::runtime_error(
                        "Licensing limitation hit - maximum amount of active power devices allowed in "
                        "license reached.");
                }
            }
            // store asset to db
            asset.create();
            // activate asset
            if (requestActivation) {
                try {
                    asset.activate();
                } catch (std::exception& e) {
                    // if activation fails, delete asset
                    fty::AssetImpl::deleteList({asset.getInternalName()}, false);
                    throw std::runtime_error(e.what());
                }
            }

            zmsg_addstr(reply, "OK");
            zmsg_addstr(reply, asset.getInternalName().c_str());

            auto notification = fty::assetutils::createMessage(FTY_ASSET_SUBJECT_CREATED, "",
                server.getAgentNameNg(), "", messagebus::STATUS_OK, fty::Asset::toJson(asset));
            server.sendNotification(notification);
        } else if (streq(operation, "update")) {
            fty::AssetImpl currentAsset(asset.getInternalName());
            // on update, add link info from current asset:
            //    as fty-proto does not carry link info, they would be deleted on update
            for(const auto& l : currentAsset.getLinkedAssets()) {
                asset.addLink(l.sourceId(), l.srcOut(), l.destIn(), l.linkType(), l.ext());
            }

            // force ID of asset to update
            log_debug("s_handle_subject_asset_manipulation(): Updating asset with internal name %s",
                asset.getInternalName().c_str());

            bool requestActivation = (currentAsset.getAssetStatus() == fty::AssetStatus::Nonactive &&
                                      asset.getAssetStatus() == fty::AssetStatus::Active);
            bool requestDeactivation = (currentAsset.getAssetStatus() == fty::AssetStatus::Active &&
                                  asset.getAssetStatus() == fty::AssetStatus::Nonactive);

            // tryUpdate is not supported in old interface
            if (!asset.isActivable()) {
                throw std::runtime_error(
                    "Licensing limitation hit - maximum amount of active power devices allowed in "
                    "license reached.");
            }
            // store asset to db
            asset.update();
            // activate asset
            if (requestActivation) {
                try {
                    asset.activate();
                } catch (std::exception& e) {
                    // if activation fails, set status to nonactive
                    asset.setAssetStatus(fty::AssetStatus::Nonactive);
                    asset.update();
                    throw std::runtime_error(e.what());
                }
            } else if(requestDeactivation) {
                asset.deactivate();
            }

            asset.load();

            zmsg_addstr(reply, "OK");
            zmsg_addstr(reply, asset.getInternalName().c_str());

            // before update
            cxxtools::SerializationInfo tmpSi;
            tmpSi <<= currentAsset;

            cxxtools::SerializationInfo si;
            cxxtools::SerializationInfo& before = si.addMember("");
            before.setCategory(cxxtools::SerializationInfo::Category::Object);
            before = tmpSi;
            before.setName("before");

            // after update
            tmpSi.clear();
            tmpSi <<= asset;

            cxxtools::SerializationInfo& after = si.addMember("");
            after.setCategory(cxxtools::SerializationInfo::Category::Object);
            after = tmpSi;
            after.setName("after");

            auto notification = fty::assetutils::createMessage(FTY_ASSET_SUBJECT_UPDATED, "",
                server.getAgentNameNg(), "", messagebus::STATUS_OK, JSON::writeToString(si, false));
            server.sendNotification(notification);
        } else {
            // unknown op
            log_error("%s:\tASSET_MANIPULATION: asset operation %s is not implemented", client_name.c_str(),
                operation);
            zmsg_addstr(reply, "ERROR");
            zmsg_addstr(reply, "OPERATION_NOT_IMPLEMENTED");
        }
    } catch (const std::exception& e) {
        log_error("exception reached: %s", e.what());
        fty_proto_print(proto);
        zmsg_destroy(&reply); // reinit for error
        reply = zmsg_new();
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr(reply, e.what());
    }

    mlm_client_sendto(const_cast<mlm_client_t*>(server.getMailboxClient()),
        mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient())), "ASSET_MANIPULATION", NULL,
        5000, &reply);

    fty_proto_destroy(&proto);
    zmsg_destroy(&reply);
}

static void s_update_topology(const fty::AssetServer& server, fty_proto_t* msg)
{
    assert (msg);

    if (!streq(fty_proto_operation(msg), FTY_PROTO_ASSET_OP_UPDATE)) {
        log_info("%s:\tIgnore: '%s' on '%s'", server.getAgentName().c_str(), fty_proto_operation(msg),
            fty_proto_name(msg));
        return;
    }
    // select assets, that were affected by the change
    std::set<std::string>    empty;
    std::vector<std::string> asset_names;
    int rv = select_assets_by_container(fty_proto_name(msg), empty, asset_names, server.getTestMode());
    if (rv != 0) {
        log_warning("%s:\tCannot select assets in container '%s'", server.getAgentName().c_str(),
            fty_proto_name(msg));
        return;
    }

    // For every asset we need to form new message!
    for (const auto& asset_name : asset_names) {
        send_create_or_update_asset(server, asset_name, FTY_PROTO_ASSET_OP_UPDATE, true);
    }
}

static void s_repeat_all(const fty::AssetServer& server, const std::set<std::string>& assets_to_publish)
{
    std::vector<std::string>               asset_names;
    std::function<void(const tntdb::Row&)> cb = [&asset_names, &assets_to_publish](const tntdb::Row& row) {
        std::string foo;
        row["name"].get(foo);
        if (assets_to_publish.size() == 0)
            asset_names.push_back(foo);
        else if (assets_to_publish.count(foo) == 1)
            asset_names.push_back(foo);
    };

    // select all assets
    int rv = select_assets(cb, server.getTestMode());
    if (rv != 0) {
        log_warning("%s:\tCannot list all assets", server.getAgentName().c_str());
        return;
    }

    // For every asset we need to form new message!
    for (const auto& asset_name : asset_names) {
        send_create_or_update_asset(server, asset_name, FTY_PROTO_ASSET_OP_UPDATE, true);
    }
}

static void s_repeat_all(const fty::AssetServer& server)
{
    return s_repeat_all(server, {});
}

void handle_incoming_limitations(fty::AssetServer& server, fty_proto_t* metric)
{
    // subject matches type.name, so checking those should be sufficient
    assert (fty_proto_id(metric) == FTY_PROTO_METRIC);
    if (streq(fty_proto_name(metric), "rackcontroller-0")) {
        if (streq(fty_proto_type(metric), "configurability.global")) {
            log_debug("Setting configurability/global to %s.", fty_proto_value(metric));
            server.setGlobalConfigurability(atoi(fty_proto_value(metric)));
        }
    }
}

void fty_asset_server(zsock_t* pipe, void* args)
{
    if (!args) {
        log_error("args is NULL");
        return;
    }

    // new messagebus interfaces (-ng suffix)
    fty::AssetServer server;
    server.setAgentName(static_cast<char*>(args));
    server.setAgentNameNg(server.getAgentName() + "-ng");
    // set-up SRR
    server.setSrrAgentName(server.getAgentName() + "-srr");
    //server.initSrr(FTY_ASSET_SRR_QUEUE);

    zpoller_t* poller = zpoller_new(
        pipe,
        mlm_client_msgpipe(const_cast<mlm_client_t*>(server.getMailboxClient())),
        mlm_client_msgpipe(const_cast<mlm_client_t*>(server.getStreamClient())),
        NULL);
    if (!poller) {
        log_error("poller new failed");
        return;
    }

    log_info("%s:\tStarted", server.getAgentName().c_str());

    zsock_signal(pipe, 0);

    while (!zsys_interrupted) {

        const int POLL_TIMEOUT_S = 30;
        void* which = zpoller_wait(poller, POLL_TIMEOUT_S * 1000);

        if (which == NULL) {
            //log_debug("which = NULL");
            if (zpoller_terminated(poller) || zsys_interrupted)
                break;
            continue;
        }
        else if (which == pipe) {
            //log_debug("which = PIPE"); }
            zmsg_t* msg = zmsg_recv(pipe);
            char*   cmd = zmsg_popstr(msg);
            log_debug("%s:\tActor command=%s", server.getAgentName().c_str(), cmd);

            if (streq(cmd, "$TERM")) {
                log_info("%s:\tGot $TERM", server.getAgentName().c_str());
                zstr_free(&cmd);
                zmsg_destroy(&msg);
                break; // while
            } else if (streq(cmd, "CONNECTSTREAM")) {
                char* endpoint = zmsg_popstr(msg);
                server.setStreamEndpoint(endpoint);

                char* stream_name = zsys_sprintf("%s-stream", server.getAgentName().c_str());
                int rv = mlm_client_connect(const_cast<mlm_client_t*>(server.getStreamClient()),
                    server.getStreamEndpoint().c_str(), 1000, stream_name);
                if (rv == -1) {
                    log_error("%s:\tCan't connect to malamute endpoint '%s'", stream_name,
                        server.getStreamEndpoint().c_str());
                }

                // new interface
                server.createPublisherClientNg(); // notifications
                server.connectPublisherClientNg();

                zstr_free(&endpoint);
                zstr_free(&stream_name);
                zsock_signal(pipe, 0);
            } else if (streq(cmd, "PRODUCER")) {
                char* stream = zmsg_popstr(msg);
                server.setTestMode(streq(stream, "ASSETS-TEST"));
                int rv = mlm_client_set_producer(const_cast<mlm_client_t*>(server.getStreamClient()), stream);
                if (rv == -1) {
                    log_error(
                        "%s:\tCan't set producer on stream '%s'", server.getAgentName().c_str(), stream);
                }
                zstr_free(&stream);
                zsock_signal(pipe, 0);
            } else if (streq(cmd, "CONSUMER")) {
                char* stream  = zmsg_popstr(msg);
                char* pattern = zmsg_popstr(msg);
                int rv = mlm_client_set_consumer(
                    const_cast<mlm_client_t*>(server.getStreamClient()), stream, pattern);
                if (rv == -1) {
                    log_error("%s:\tCan't set consumer on stream '%s', '%s'", server.getAgentName().c_str(),
                        stream, pattern);
                }
                zstr_free(&pattern);
                zstr_free(&stream);
                zsock_signal(pipe, 0);
            } else if (streq(cmd, "CONNECTMAILBOX")) {
                char* endpoint = zmsg_popstr(msg);
                server.setMailboxEndpoint(endpoint);
                server.setSrrEndpoint(endpoint);

                int rv = mlm_client_connect(const_cast<mlm_client_t*>(server.getMailboxClient()),
                    server.getMailboxEndpoint().c_str(), 1000, server.getAgentName().c_str());
                if (rv == -1) {
                    log_error("%s:\tCan't connect to malamute endpoint '%s'", server.getAgentName().c_str(),
                        server.getMailboxEndpoint().c_str());
                }

                // new interface
                server.createMailboxClientNg(); // queue
                server.connectMailboxClientNg();
                server.receiveMailboxClientNg(FTY_ASSET_MAILBOX);
                server.initSrr(FTY_ASSET_SRR_QUEUE);

                zstr_free(&endpoint);
                zsock_signal(pipe, 0);
            } else if (streq(cmd, "REPEAT_ALL")) {
                s_repeat_all(server);
                log_debug("%s:\tREPEAT_ALL end", server.getAgentName().c_str());
            } else {
                log_info("%s:\tUnhandled command %s", server.getAgentName().c_str(), cmd);
            }
            zstr_free(&cmd);
            zmsg_destroy(&msg);
            continue;
        }
        // This agent is a reactive agent, it reacts only on messages
        // and doesn't do anything if there are no messages
        else if (which == mlm_client_msgpipe(const_cast<mlm_client_t*>(server.getMailboxClient())))
        {
            zmsg_t* zmessage = mlm_client_recv(const_cast<mlm_client_t*>(server.getMailboxClient()));
            if (zmessage == NULL) {
                continue;
            }
            const char* sender = mlm_client_sender(const_cast<mlm_client_t*>(server.getMailboxClient()));
            const char* subject = mlm_client_subject(const_cast<mlm_client_t*>(server.getMailboxClient()));
            log_info("%s:\tMAILBOX DELIVER (sender: %s, subject: %s)", server.getAgentName().c_str(), sender, subject);

            if (streq(subject,"TOPOLOGY")) {
                s_handle_subject_topology(server, zmessage);
            } else if (streq(subject,"ASSETS_IN_CONTAINER")) {
                s_handle_subject_assets_in_container(server, zmessage);
            } else if (streq(subject,"ASSETS")) {
                s_handle_subject_assets(server, zmessage);
            } else if (streq(subject,"ENAME_FROM_INAME")) {
                s_handle_subject_ename_from_iname(server, zmessage);
            } else if (streq(subject,"REPUBLISH")) {

                char* asset = zmsg_popstr(zmessage);
                if (!asset || streq(asset, "$all")) {
                    s_repeat_all(server);
                } else {
                    std::set<std::string> assets_to_publish;
                    while (asset) {
                        assets_to_publish.insert(asset);
                        zstr_free(&asset);
                        asset = zmsg_popstr(zmessage);
                    }
                    s_repeat_all(server, assets_to_publish);
                }
                zstr_free(&asset);
                // do not reply anything (requesters don't expect an answer)

            } else if (streq(subject,"ASSET_MANIPULATION")) {
                s_handle_subject_asset_manipulation(server, &zmessage);
            } else if (streq(subject,"ASSET_DETAIL")) {
                s_handle_subject_asset_detail(server, &zmessage);
            } else {
                log_info("%s:\tUnexpected subject '%s'", server.getAgentName().c_str(), subject);
            }
            zmsg_destroy(&zmessage);
        }
        else if (which == mlm_client_msgpipe(const_cast<mlm_client_t*>(server.getStreamClient())))
        {
            zmsg_t* zmessage = mlm_client_recv(const_cast<mlm_client_t*>(server.getStreamClient()));
            if (zmessage == NULL) {
                continue;
            }

            if (fty_proto_is(zmessage)) {
                fty_proto_t* bmsg = fty_proto_decode(&zmessage);

                if (fty_proto_id(bmsg) == FTY_PROTO_ASSET) {
                    log_debug("%s:\tSTREAM DELIVER (PROTO_ASSET)", server.getAgentName().c_str());
                    s_update_topology(server, bmsg);
                } else if (fty_proto_id(bmsg) == FTY_PROTO_METRIC) {
                    log_debug("%s:\tSTREAM DELIVER (PROTO_METRIC)", server.getAgentName().c_str());
                    handle_incoming_limitations(server, bmsg);
                }
                fty_proto_destroy(&bmsg);
            }
            zmsg_destroy(&zmessage);
        }
        else {
            //log_debug("OTHER");
        }
    }

    log_info("%s:\tended", server.getAgentName().c_str());
    zpoller_destroy(&poller);
}

//  --------------------------------------------------------------------------
//  Self test of this class

// stores correlationID : asset JSON for each message received
static std::map<std::string, std::string> assetTestMap;

// IMPORTANT NOTE:
// here, we need privileges to write in DB (runtime)
// we only logs succes or error... no catch2 macros here
static void s_test_asset_mailbox_handler(const messagebus::Message& msg)
{
    std::function<std::string(const messagebus::Message&)> msg2str = [] (const messagebus::Message& m)
    {
        std::ostringstream oss;
        oss << "metaData (" << m.metaData().size() << "):" << std::endl;
        for (const auto& i : m.metaData()) {
            oss << i.first << ": " << i.second << std::endl;
        }
        oss << "userData (" << m.userData().size() << "):" << std::endl;
        for (const auto& s : m.userData()) {
            oss << s << std::endl;
        }
        return oss.str();
    };

    std::cout << "s_test_asset_mailbox_handler" << std::endl;
    std::cout << "msg: " << msg2str(msg) << std::endl;

    try {
        std::string msgSubject = msg.metaData().find(messagebus::Message::SUBJECT)->second;
        if (msgSubject == FTY_ASSET_SUBJECT_CREATE) {
            fty::Asset msgAsset;
            fty::Asset::fromJson(msg.userData().back(), msgAsset);

            fty::Asset mapAsset;
            fty::Asset::fromJson(
                assetTestMap.find(msg.metaData().find(messagebus::Message::CORRELATION_ID)->second)->second,
                mapAsset);

            std::cout << "msgAsset: " << std::endl << fty::Asset::toJson(msgAsset) << std::endl;
            std::cout << "mapAsset: " << std::endl << fty::Asset::toJson(mapAsset) << std::endl;

            if (msgAsset.getInternalName() == mapAsset.getInternalName()) {
                log_info("fty-asset-server-test:Test #15.1: OK");
            } else {
                log_error("fty-asset-server-test:Test #15.1: FAILED");
            }
        } else if (msgSubject == FTY_ASSET_SUBJECT_UPDATE) {
            fty::Asset msgAsset;
            fty::Asset::fromJson(msg.userData().back(), msgAsset);

            fty::Asset mapAsset;
            fty::Asset::fromJson(
                assetTestMap.find(msg.metaData().find(messagebus::Message::CORRELATION_ID)->second)->second,
                mapAsset);

            std::cout << "msgAsset: " << std::endl << fty::Asset::toJson(msgAsset) << std::endl;
            std::cout << "mapAsset: " << std::endl << fty::Asset::toJson(mapAsset) << std::endl;

            if (msgAsset.getInternalName() == mapAsset.getInternalName()) {
                log_info("fty-asset-server-test:Test #15.2: OK");
            } else {
                log_error("fty-asset-server-test:Test #15.2: FAILED");
            }
        } else if (msgSubject == FTY_ASSET_SUBJECT_GET) {
            std::string assetJson = msg.userData().back();
            fty::Asset  msgAsset;
            fty::Asset::fromJson(assetJson, msgAsset);

            std::cout << "msgAsset: " << std::endl << fty::Asset::toJson(msgAsset) << std::endl;

            std::string assetName = msgAsset.getInternalName();

            if (assetTestMap.find(msg.metaData().find(messagebus::Message::CORRELATION_ID)->second)->second ==
                assetName) {
                log_info("fty-asset-server-test:Test #15.3: OK");
            } else {
                log_error("fty-asset-server-test:Test #15.3: FAILED");
            }
        } else {
            log_error("fty-asset-server-test:Invalid subject %s", msgSubject.c_str());
        }
    } catch (const std::exception& e) {
        log_error("s_test_asset_mailbox_handler (e: %s)", e.what());
    }
}

//selftest main

void fty_asset_server_test(bool /*verbose*/)
{
    log_debug("Setting test mode to true");
    g_testMode = true;

    std::cout << " * fty_asset_server:" << std::endl;

    //  @selftest
    // Test #1:  Simple create/destroy test
    std::cout << "fty-asset-server-test:Test #1" << std::endl;
    {
        fty::AssetServer server;
    }
    std::cout << "fty-asset-server-test:Test #1: OK" << std::endl;

    std::string rnd_name = "1234";
    {
        timeval t;
        gettimeofday(&t, NULL);
        srand(static_cast<unsigned int>(t.tv_sec * t.tv_usec));
        std::cerr << "################### " << t.tv_sec * t.tv_usec << std::endl;
        rnd_name = std::to_string(rand());
    }

    const std::string endpoint = "inproc://fty_asset_server-test";
    const std::string client_name = "fty-asset-" + rnd_name;
    const std::string asset_server_test_name = "asset_agent_test-" + rnd_name;
    std::cout << "endpoint: " << endpoint << std::endl;
    std::cout << "client_name: " << client_name << std::endl;
    std::cout << "asset_server_test_name: " << asset_server_test_name << std::endl;

    std::cout << "create server..." << std::endl;
    zactor_t* server = zactor_new(mlm_server, static_cast<void*>( const_cast<char*>("Malamute")));
    assert (server != NULL);
    zstr_sendx(server, "BIND", endpoint.c_str(), NULL);

    std::cout << "create ui..." << std::endl;
    mlm_client_t* ui = mlm_client_new();
    assert(ui != NULL);
    mlm_client_connect(ui, endpoint.c_str(), 5000, client_name.c_str());
    mlm_client_set_producer(ui, "ASSETS-TEST");
    mlm_client_set_consumer(ui, "ASSETS-TEST", ".*");

    std::cout << "create asset_server..." << std::endl;
    zactor_t* asset_server = zactor_new(fty_asset_server, static_cast<void*>( const_cast<char*>(asset_server_test_name.c_str())));
    assert(asset_server != NULL);
    zstr_sendx(asset_server, "CONNECTSTREAM", endpoint.c_str(), NULL);
    zsock_wait(asset_server);
    zstr_sendx(asset_server, "PRODUCER", "ASSETS-TEST", NULL);
    zsock_wait(asset_server);
    zstr_sendx(asset_server, "CONSUMER", "ASSETS-TEST", ".*", NULL);
    zsock_wait(asset_server);
    zstr_sendx(asset_server, "CONSUMER", "LICENSING-ANNOUNCEMENTS-TEST", ".*", NULL);
    zsock_wait(asset_server);
    zstr_sendx(asset_server, "CONNECTMAILBOX", endpoint.c_str(), NULL);
    zsock_wait(asset_server);

    static const char* asset_name = TEST_INAME;

    std::cout << "Test #2: subject ASSET_MANIPULATION, message fty_proto_t *asset" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #2");
        const char* subject = "ASSET_MANIPULATION";
        zhash_t*    aux     = zhash_new();
        zhash_insert(aux, "type", static_cast<void*>( const_cast<char*>("datacenter")));
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>("N_A")));
        zmsg_t* msg = fty_proto_encode_asset(aux, asset_name, FTY_PROTO_ASSET_OP_CREATE, NULL);
        zmsg_pushstrf(msg, "%s", "READWRITE");
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        zhash_destroy(&aux);
        assert (rv == 0);
        zmsg_t* reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            char* str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
            str = zmsg_popstr(reply);
            // assert (streq(str, asset_name));
            zstr_free(&str);
            zmsg_destroy(&reply);
        } else {
            assert (fty_proto_is(reply));
            fty_proto_t* fmsg             = fty_proto_decode(&reply);
            std::string  expected_subject = "unknown.unknown@";
            expected_subject.append(asset_name);
            // assert (streq(mlm_client_subject(ui), expected_subject.c_str()));
            assert (streq(fty_proto_operation(fmsg), FTY_PROTO_ASSET_OP_CREATE));
            fty_proto_destroy(&fmsg);
            zmsg_destroy(&reply);
        }

        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            char* str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
            str = zmsg_popstr(reply);
            // assert (streq(str, asset_name));
            zstr_free(&str);
            zmsg_destroy(&reply);
        } else {
            assert (fty_proto_is(reply));
            fty_proto_t* fmsg             = fty_proto_decode(&reply);
            std::string  expected_subject = "unknown.unknown@";
            expected_subject.append(asset_name);
            // assert (streq(mlm_client_subject(ui), expected_subject.c_str()));
            assert (streq(fty_proto_operation(fmsg), FTY_PROTO_ASSET_OP_CREATE));
            fty_proto_destroy(&fmsg);
            zmsg_destroy(&reply);
        }
        log_info("fty-asset-server-test:Test #2: OK");
    }

    std::cout << "Test #3: message fty_proto_t *asset" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #3");
        zmsg_t* msg = fty_proto_encode_asset(NULL, asset_name, FTY_PROTO_ASSET_OP_UPDATE, NULL);
        int rv  = mlm_client_send(ui, "update-test", &msg);
        assert (rv == 0);
        zclock_sleep(200);
        log_info("fty-asset-server-test:Test #3: OK");
    }

    std::cout << "Test #4: subject TOPOLOGY, message POWER" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #4");
        const char* subject = "TOPOLOGY";
        const char* command = "POWER";
        const char* uuid    = "123456";
        zmsg_t*     msg     = zmsg_new();
        zmsg_addstr(msg, "REQUEST");
        zmsg_addstr(msg, uuid);
        zmsg_addstr(msg, command);
        zmsg_addstr(msg, asset_name);
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        assert (rv == 0);
        zmsg_t* reply = mlm_client_recv(ui);
        assert (streq(mlm_client_subject(ui), subject));
        assert (zmsg_size(reply) == 5);
        char* str = zmsg_popstr(reply);
        assert (streq(str, uuid));
        zstr_free(&str);
        str = zmsg_popstr(reply);
        assert (streq(str, "REPLY"));
        zstr_free(&str);
        str = zmsg_popstr(reply);
        assert (streq(str, command));
        zstr_free(&str);
        str = zmsg_popstr(reply);
        // assert (streq(str, asset_name));
        zstr_free(&str);
        str = zmsg_popstr(reply);
        assert (streq(str, "OK"));
        zstr_free(&str);
        zmsg_destroy(&reply);
        log_info("fty-asset-server-test:Test #4: OK");
    }
    std::cout << "Test #5: subject ASSETS_IN_CONTAINER, message GET" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #5");
        const char* subject = "ASSETS_IN_CONTAINER";
        const char* command = "GET";
        zmsg_t*     msg     = zmsg_new();
        zmsg_addstr(msg, command);
        zmsg_addstr(msg, asset_name);
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        assert (rv == 0);
        zmsg_t* reply = mlm_client_recv(ui);
        assert (streq(mlm_client_subject(ui), subject));
        assert (zmsg_size(reply) == 1);
        char* str = zmsg_popstr(reply);
        assert (streq(str, "OK"));
        zstr_free(&str);
        zmsg_destroy(&reply);
        log_info("fty-asset-server-test:Test #5: OK");
    }
    std::cout << "Test #6: subject ASSETS, message GET" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #6");
        const char* subject = "ASSETS";
        const char* command = "GET";
        zmsg_t*     msg     = zmsg_new();
        zmsg_addstr(msg, command);
        zmsg_addstr(msg, "UUID");
        zmsg_addstr(msg, asset_name);
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        assert (rv == 0);
        zmsg_t* reply = mlm_client_recv(ui);
        assert (streq(mlm_client_subject(ui), subject));
        assert (zmsg_size(reply) == 2);
        char* uuid = zmsg_popstr(reply);
        assert (streq(uuid, "UUID"));
        char* str = zmsg_popstr(reply);
        assert (streq(str, "OK"));
        zstr_free(&str);
        zstr_free(&uuid);
        zmsg_destroy(&reply);
        log_info("fty-asset-server-test:Test #6: OK");
    }
    std::cout << "Test #7: message REPEAT_ALL" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #7");
        const char* command = "REPEAT_ALL";
        int rv      = zstr_sendx(asset_server, command, NULL);
        assert (rv == 0);
        zclock_sleep(200);
        log_info("fty-asset-server-test:Test #7: OK");
    }
    std::cout << "Test #8: subject REPUBLISH, message $all" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #8");
        const char* subject = "REPUBLISH";
        zmsg_t*     msg     = zmsg_new();
        zmsg_addstr(msg, "$all");
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        assert (rv == 0);
        zclock_sleep(200);
        log_info("fty-asset-server-test:Test #8: OK");
    }
    std::cout << "Test #9: subject ASSET_DETAIL, message GET/<iname>" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #9");
        const char* subject = "ASSET_DETAIL";
        const char* command = "GET";
        const char* uuid    = "UUID-0000-TEST";
        zmsg_t*     msg     = zmsg_new();
        zmsg_addstr(msg, command);
        zmsg_addstr(msg, uuid);
        zmsg_addstr(msg, asset_name);
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        assert (rv == 0);
        zmsg_t* reply    = mlm_client_recv(ui);
        char*   rcv_uuid = zmsg_popstr(reply);
        assert (0 == strcmp(rcv_uuid, uuid));
        assert (fty_proto_is(reply));
        fty_proto_t* freply = fty_proto_decode(&reply);
        const char*  str    = fty_proto_name(freply);
        // assert (streq(str, asset_name));
        str = fty_proto_operation(freply);
        assert (streq(str, FTY_PROTO_ASSET_OP_UPDATE));
        fty_proto_destroy(&freply);
        zstr_free(&rcv_uuid);
        log_info("fty-asset-server-test:Test #9: OK");
    }

    std::cout << "Test #10: subject ENAME_FROM_INAME, message <iname>" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #10");
        const char* subject     = "ENAME_FROM_INAME";
        zmsg_t*     msg         = zmsg_new();
        zmsg_addstr(msg, asset_name);
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        assert (rv == 0);
        zmsg_t* reply = mlm_client_recv(ui);
        assert (zmsg_size(reply) == 2);
        char* str = zmsg_popstr(reply);
        assert (streq(str, "OK"));
        zstr_free(&str);
        str = zmsg_popstr(reply);
        assert (streq(str, TEST_ENAME));
        zstr_free(&str);
        zmsg_destroy(&reply);
        log_info("fty-asset-server-test:Test #10: OK");
    }

    zactor_t* autoupdate_server = zactor_new(fty_asset_autoupdate_server, static_cast<void*>( const_cast<char*>("asset-autoupdate-test")));
    zstr_sendx(autoupdate_server, "CONNECT", endpoint.c_str(), NULL);
    zsock_wait(autoupdate_server);
    zstr_sendx(autoupdate_server, "PRODUCER", "ASSETS-TEST", NULL);
    zsock_wait(autoupdate_server);
    zstr_sendx(autoupdate_server, "ASSET_AGENT_NAME", asset_server_test_name.c_str(), NULL);

    std::cout << "Test #11: message WAKEUP" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #11");
        const char* command = "WAKEUP";
        int rv      = zstr_sendx(autoupdate_server, command, NULL);
        assert (rv == 0);
        zclock_sleep(200);
        log_info("fty-asset-server-test:Test #11: OK");
    }

    std::cout << "Test #12: test licensing limitations" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #12");
        // try to create asset when configurability is enabled
        const char* subject = "ASSET_MANIPULATION";
        zhash_t*    aux     = zhash_new();
        zhash_insert(aux, "type", static_cast<void*>( const_cast<char*>("datacenter")));
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>("N_A")));
        zmsg_t* msg = fty_proto_encode_asset(aux, asset_name, FTY_PROTO_ASSET_OP_CREATE, NULL);
        zmsg_pushstrf(msg, "%s", "READWRITE");
        int rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        zclock_sleep(200);
        zhash_destroy(&aux);
        assert (rv == 0);
        char*   str   = NULL;
        zmsg_t* reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        // disable configurability
        mlm_client_set_producer(ui, "LICENSING-ANNOUNCEMENTS-TEST");
        zmsg_t* smsg = fty_proto_encode_metric(
            NULL, static_cast<uint64_t>(time(NULL)), 24 * 60 * 60, "configurability.global", "rackcontroller-0", "0", "");
        mlm_client_send(ui, "configurability.global@rackcontroller-0", &smsg);
        zclock_sleep(200);
        // try to create asset when configurability is disabled
        aux = zhash_new();
        zhash_insert(aux, "type", static_cast<void*>( const_cast<char*>("datacenter")));
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>("N_A")));
        msg = fty_proto_encode_asset(aux, asset_name, FTY_PROTO_ASSET_OP_CREATE, NULL);
        zmsg_pushstrf(msg, "%s", "READWRITE");
        rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        zclock_sleep(200);
        zhash_destroy(&aux);
        assert (rv == 0);
        reply = mlm_client_recv(ui);
        assert (streq(mlm_client_subject(ui), subject));
        assert (zmsg_size(reply) == 2);
        str = zmsg_popstr(reply);
        assert (streq(str, "ERROR"));
        zstr_free(&str);
        str = zmsg_popstr(reply);
        assert (streq(str, "Licensing limitation hit - asset manipulation is prohibited."));
        zstr_free(&str);
        zmsg_destroy(&reply);
        // enable configurability again, but set limit to power devices
        smsg = fty_proto_encode_metric(
            NULL, static_cast<uint64_t>(time(NULL)), 24 * 60 * 60, "configurability.global", "rackcontroller-0", "1", "");
        mlm_client_send(ui, "configurability.global@rackcontroller-0", &smsg);
        smsg = fty_proto_encode_metric(
            NULL, static_cast<uint64_t>(time(NULL)), 24 * 60 * 60, "power_nodes.max_active", "rackcontroller-0", "3", "");
        mlm_client_send(ui, "power_nodes.max_active@rackcontroller-0", &smsg);
        zclock_sleep(300);
        // send power devices
        aux = zhash_new();
        zhash_insert(aux, "type", static_cast<void*>( const_cast<char*>("device")));
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>("epdu")));
        zhash_insert(aux, "status", static_cast<void*>( const_cast<char*>("active")));
        msg = fty_proto_encode_asset(aux, "test1", FTY_PROTO_ASSET_OP_UPDATE, NULL);
        zmsg_pushstrf(msg, "%s", "READWRITE");
        rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        if (aux)
            zhash_destroy(&aux);
        zclock_sleep(200);
        assert (rv == 0);
        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        aux = zhash_new();
        zhash_autofree(aux);
        zhash_insert(aux, "type", static_cast<void*>( const_cast<char*>("device")));
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>("epdu")));
        zhash_insert(aux, "status", static_cast<void*>( const_cast<char*>("active")));
        msg = fty_proto_encode_asset(aux, "test2", FTY_PROTO_ASSET_OP_UPDATE, NULL);
        zmsg_pushstrf(msg, "%s", "READWRITE");
        rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        if (aux)
            zhash_destroy(&aux);
        zclock_sleep(1000);
        assert (rv == 0);
        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        aux = zhash_new();
        zhash_autofree(aux);
        zhash_insert(aux, "type", static_cast<void*>( const_cast<char*>("device")));
        zhash_insert(aux, "subtype", static_cast<void*>( const_cast<char*>("epdu")));
        zhash_insert(aux, "status", static_cast<void*>( const_cast<char*>("active")));
        msg = fty_proto_encode_asset(aux, "test3", FTY_PROTO_ASSET_OP_UPDATE, NULL);
        zmsg_pushstrf(msg, "%s", "READWRITE");
        rv = mlm_client_sendto(ui, asset_server_test_name.c_str(), subject, NULL, 5000, &msg);
        if (aux)
            zhash_destroy(&aux);
        zclock_sleep(200);
        assert (rv == 0);

        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message

        reply = mlm_client_recv(ui);
        if (!fty_proto_is(reply)) {
            assert (streq(mlm_client_subject(ui), subject));
            assert (zmsg_size(reply) == 2);
            str = zmsg_popstr(reply);
            assert (streq(str, "OK"));
            zstr_free(&str);
        }
        zmsg_destroy(&reply); // throw away stream message
    }

    std::cout << "Test #13: asset conversion to json" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #13");

        fty::Asset asset;
        asset.setInternalName("dc-0");
        asset.setAssetStatus(fty::AssetStatus::Nonactive);
        asset.setAssetType(fty::TYPE_DEVICE);
        asset.setAssetSubtype(fty::SUB_UPS);
        asset.setParentIname("abc123");
        asset.setExtEntry("testKey", "testValue");
        asset.setPriority(4);

        std::string jsonStr = fty::Asset::toJson(asset);

        fty::Asset asset2;
        fty::Asset::fromJson(jsonStr, asset2);

        assert (asset == asset2);

        log_debug("fty-asset-server-test:Test #13 OK");
    }

    std::cout << "Test #14: asset conversion to fty-proto" << std::endl;
    {
        log_debug("fty-asset-server-test:Test #14");

        fty::Asset asset;
        asset.setInternalName("dc-0");
        asset.setAssetStatus(fty::AssetStatus::Nonactive);
        asset.setAssetType(fty::TYPE_DEVICE);
        asset.setAssetSubtype(fty::SUB_UPS);
        asset.setParentIname("test-parent");
        asset.setExtEntry("testKey", "testValue");
        asset.setPriority(4);

        asset.dump(std::cout);

        fty_proto_t* p = fty::Asset::toFtyProto(asset, "UPDATE", true);

        fty::Asset asset2;

        fty::Asset::fromFtyProto(p, asset2, false, true);
        fty_proto_destroy(&p);

        asset2.dump(std::cout);

        assert (asset == asset2);

        log_debug("fty-asset-server-test:Test #14 OK");
    }

    std::cout << "Test #15: new generation asset interface" << std::endl;
    {
        static const char* FTY_ASSET_TEST_Q   = "FTY.Q.ASSET.TEST";
        static const char* FTY_ASSET_TEST_PUB = "test-publisher";
        static const char* FTY_ASSET_TEST_REC = "test-receiver";

        const std::string FTY_ASSET_TEST_MAIL_NAME = asset_server_test_name + "-ng";

        log_debug("fty-asset-server-test:Test #15");

        std::unique_ptr<messagebus::MessageBus> publisher(
            messagebus::MlmMessageBus(endpoint, FTY_ASSET_TEST_PUB));
        std::unique_ptr<messagebus::MessageBus> receiver(
            messagebus::MlmMessageBus(endpoint, FTY_ASSET_TEST_REC));

        publisher->connect();

        receiver->connect();
        receiver->receive(FTY_ASSET_TEST_Q, s_test_asset_mailbox_handler);

        // test asset
        fty::Asset asset;
        asset.setInternalName("test-asset");
        asset.setAssetStatus(fty::AssetStatus::Active);
        asset.setAssetType("device");
        asset.setAssetSubtype("ups");
        asset.setParentIname("");
        asset.setPriority(4);
        asset.setExtEntry("name", "Test asset");

        std::cout << "asset: " << std::endl << fty::Asset::toJson(asset) << std::endl;

        messagebus::Message msg;

        // test create
        msg.metaData().clear();
        msg.metaData().emplace(messagebus::Message::CORRELATION_ID, messagebus::generateUuid());
        msg.metaData().emplace(messagebus::Message::SUBJECT, FTY_ASSET_SUBJECT_CREATE);
        msg.metaData().emplace(messagebus::Message::FROM, FTY_ASSET_TEST_REC);
        msg.metaData().emplace(messagebus::Message::TO, FTY_ASSET_TEST_MAIL_NAME);
        msg.metaData().emplace(messagebus::Message::REPLY_TO, FTY_ASSET_TEST_Q);
        msg.metaData().emplace(METADATA_TRY_ACTIVATE, "true");
        msg.metaData().emplace(METADATA_NO_ERROR_IF_EXIST, "true");

        msg.userData().clear();
        msg.userData().push_back(fty::Asset::toJson(asset));

        assetTestMap.emplace(
            msg.metaData().find(messagebus::Message::CORRELATION_ID)->second, fty::Asset::toJson(asset));

        log_info("fty-asset-server-test:Test #15.1: send CREATE message");
        publisher->sendRequest(FTY_ASSET_MAILBOX, msg);
        zclock_sleep(200);

        // test update
        msg.metaData().clear();
        msg.metaData().emplace(messagebus::Message::CORRELATION_ID, messagebus::generateUuid());
        msg.metaData().emplace(messagebus::Message::SUBJECT, FTY_ASSET_SUBJECT_UPDATE);
        msg.metaData().emplace(messagebus::Message::FROM, FTY_ASSET_TEST_REC);
        msg.metaData().emplace(messagebus::Message::TO, FTY_ASSET_TEST_MAIL_NAME);
        msg.metaData().emplace(messagebus::Message::REPLY_TO, FTY_ASSET_TEST_Q);
        msg.metaData().emplace(METADATA_TRY_ACTIVATE, "true");

        msg.userData().clear();

        assetTestMap.emplace(
            msg.metaData().find(messagebus::Message::CORRELATION_ID)->second, fty::Asset::toJson(asset));

        log_info("fty-asset-server-test:Test #15.2: send UPDATE message");
        publisher->sendRequest(FTY_ASSET_MAILBOX, msg);
        zclock_sleep(200);
        // test get
        msg.metaData().clear();
        msg.metaData().emplace(messagebus::Message::CORRELATION_ID, messagebus::generateUuid());
        msg.metaData().emplace(messagebus::Message::SUBJECT, FTY_ASSET_SUBJECT_GET);
        msg.metaData().emplace(messagebus::Message::FROM, FTY_ASSET_TEST_REC);
        msg.metaData().emplace(messagebus::Message::TO, FTY_ASSET_TEST_MAIL_NAME);
        msg.metaData().emplace(messagebus::Message::REPLY_TO, FTY_ASSET_TEST_Q);
        msg.metaData().emplace(METADATA_TRY_ACTIVATE, "true");

        msg.userData().clear();
        msg.userData().push_back("test-asset");

        assetTestMap.emplace(msg.metaData().find(messagebus::Message::CORRELATION_ID)->second, "test-asset");

        log_info("fty-asset-server-test:Test #15.3: send GET message");
        publisher->sendRequest(FTY_ASSET_MAILBOX, msg);
        zclock_sleep(200);
    }

    zactor_destroy(&autoupdate_server);
    zactor_destroy(&asset_server);
    mlm_client_destroy(&ui);
    zactor_destroy(&server);

    //  @end
    std::cout << "fty_asset_server: OK" << std::endl;
}
