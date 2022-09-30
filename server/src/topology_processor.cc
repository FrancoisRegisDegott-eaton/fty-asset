/*  =========================================================================
    topology_processor - Topology requests processor

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
    topology_processor - Topology requests processor
@discuss
@end
*/

#include "topology_processor.h"
#include "topology_power.h"
#include "topology_location.h"
#include "topology_input_powerchain.h"

#include <fty_log.h>
#include <fty_common.h>

#include <cxxtools/serializationinfo.h>
#include <cxxtools/jsondeserializer.h>
#include <cxxtools/jsonserializer.h>
#include <map>

// fwd decl.
static int json_string_beautify (std::string & s);
static int si_to_string (const cxxtools::SerializationInfo & si, std::string & s, bool beautify);
static int string_to_si (const std::string & s, cxxtools::SerializationInfo & si);
static int si_member_value (const cxxtools::SerializationInfo & si, const std::string & member, std::string & value);

// --------------------------------------------------------------------------
// Retrieve the powerchains which powers a requested target asset
// Implementation of REST /api/v1/topology/power?[from/to/filter_dc/filter_group] (see RFC11)
// COMMAND is in {"from", "to", "filter_dc", "filter_group"} tokens set
// ASSETNAME is the subject of the command
// ERRORMSG set on failure (reason)
// On success, RESULT is valid (JSON payload)
// Returns 0 if success, else <0

int topology_power_process (const std::string & command, const std::string & assetName, std::string & result, std::string & errorMsg, bool beautify)
{
    result = "";

    std::map<std::string, std::string> param;
    param[command] = assetName;

    int r = topology_power (param, result);
    if (r != 0) {
        errorMsg = param["error"]; // reason
        log_error("topology_power() failed, r: %d, command: %s, assetName: %s",
            r, command.c_str(), assetName.c_str());
        return -1;
    }

    // beautify result, optional
    if (beautify) {
        r = json_string_beautify(result);
        if (r != 0) {
            errorMsg = TRANSLATE_ME("JSON beautification failed"); // reason
            log_error("beautification failed, r: %d, result: \n%s", r, result.c_str());
            return -2;
        }
    }

    log_debug("topology_power_process() success, command: %s, assetName: %s, result:\n%s",
        command.c_str(), assetName.c_str(), result.c_str());

    return 0; // ok
}

// --------------------------------------------------------------------------
// Retrieve the closest powerchain which powers a requested target asset
// implementation of REST /api/v1/topology/power?to (see RFC11) **filtered** on dst-id == assetName
// ASSETNAME is the target asset
// ERRORMSG set on failure (reason)
// On success, RESULT is valid (JSON payload)
// Returns 0 if success, else <0

int topology_power_to (const std::string & assetName, std::string & result, std::string & errorMsg, bool beautify)
{
    result = "";

    int r = topology_power_process("to", assetName, result, errorMsg, beautify);
    if (r != 0) {
        log_error("topology_power_process 'to' failed, r: %d", r);
        return -1;
    }

    // deserialize result to si
    cxxtools::SerializationInfo si;
    r = string_to_si(result, si);
    if (r != 0) {
        errorMsg = TRANSLATE_ME("JSON deserialization failed"); // reason
        log_error("json deserialize failed (exception reached), payload:\n%s", result.c_str());
        return -2;
    }

    // check si structure
    cxxtools::SerializationInfo *si_powerchains = si.findMember("powerchains");
    if (si_powerchains == 0) {
        errorMsg = TRANSLATE_ME("Internal error"); // reason
        log_error("powerchains member not defined, payload:\n%s", result.c_str());
        return -3;
    }
    if (si_powerchains->category() != cxxtools::SerializationInfo::Category::Array) {
        errorMsg = TRANSLATE_ME("Internal error"); // reason
        log_error("powerchains member category != Array, payload:\n%s", result.c_str());
        return -4;
    }

    result = ""; // useless, we work now on siResult

    // prepare siResult (asset-id member + powerchains array member)
    cxxtools::SerializationInfo siResult;
    siResult.addMember("asset-id") <<= assetName;
    siResult.addMember("powerchains").setCategory(cxxtools::SerializationInfo::Array);
    cxxtools::SerializationInfo *siResulPowerchains = siResult.findMember("powerchains");
    if (siResulPowerchains == 0) {
        errorMsg = TRANSLATE_ME("Internal error"); // reason
        log_error("powerchains member creation failed");
        return -5;
    }

    // powerchains member is defined (array), loop on si_powerchains items...
    // filter on dst-id == assetName, check src-id & src-socket,
    // keep entries in siResulPowerchains
    cxxtools::SerializationInfo::Iterator it;
    for (it = si_powerchains->begin(); it != si_powerchains->end(); ++it) {
        cxxtools::SerializationInfo *xsi = &(*it);
        if (xsi == 0) continue; //

        cxxtools::SerializationInfo *si_dstId = xsi->findMember("dst-id");
        if (si_dstId == 0 || si_dstId->isNull())
            { log_error("dst-id member is missing"); continue; }
        std::string dstId;
        si_dstId->getValue(dstId);
        if (dstId != assetName) continue; // filtered

        cxxtools::SerializationInfo *si_srcId, *si_srcSock;
        si_srcId = xsi->findMember("src-id");
        if (si_srcId == 0 || si_srcId->isNull())
            { log_error("src-id member is missing"); continue; }
        si_srcSock = xsi->findMember("src-socket");
        if (si_srcSock == 0 || si_srcSock->isNull())
            { log_error("src-socket member is missing"); continue; }

        // keep that entry
        siResulPowerchains->addMember("") <<= (*xsi);
    }

    // dump siResult to result
    r = si_to_string(siResult, result, beautify);
    if (r != 0) {
        errorMsg = TRANSLATE_ME("JSON serialization failed"); // reason
        log_error("serialization to JSON has failed");
        return -6;
    }

    log_debug("topology_power_to() success, assetName: %s, result:\n%s",
        assetName.c_str(), result.c_str());

    return 0; // ok
}

// --------------------------------------------------------------------------
// Retrieve location topology for a requested target asset
// Implementation of REST /api/v1/topology/location?[from/to] (see RFC11)
// COMMAND is in {"to", "from"} tokens set
// ASSETNAME is the subject of the command (can be "none" if command is "from")
// ERRORMSG set on failure (reason)
// OPTIONS:
//    if 'to'  : must be empty (no option allowed)
//    if 'from': json payload as { "recursive": <true|false>, "filter": <element_kind>, "feed_by": <asset_id> }
//               where <element_kind> is in {"rooms" , "rows", "racks", "devices", "groups"}
//               if "feed_by" is specified, "filter" *must* be set to "devices", ASSETNAME *must* not be set to "none"
//               defaults are { "recursive": false, "filter": "", "feed_by": "" }
// On success, RESULT is valid (JSON payload)
// Returns 0 if success, else <0

int topology_location_process (const std::string & command, const std::string & assetName, const std::string & options, std::string & result, std::string & errorMsg, bool beautify)
{
    result = "";

    int r;
    std::map<std::string, std::string> param;

    param[command] = assetName;

    // filter/check command, set options
    if (command == "to") {
        if (!options.empty()) {
            errorMsg = TRANSLATE_ME("'option' argument is not allowed with 'to' command"); // reason
            log_error("unexpected options for command 'to' (options: %s)", options.c_str());
            return -1;
        }
    }
    else if (command == "from") {
        // defaults
        param["recursive"] = "false";
        param["feed_by"] = "";
        param["filter"] = "";

        // parse options (ignore unreferenced entry)
        if (!options.empty()) {
            cxxtools::SerializationInfo si;
            r = string_to_si(options, si);
            if (r != 0) {
                errorMsg = TRANSLATE_ME("'option' argument describe an invalid JSON payload"); // reason
                log_error("options malformed, r: %d, options: %s", r, options.c_str());
                return -1;
            }

            std::string value;
            if (0 == si_member_value(si, "recursive", value)) param["recursive"] = value;
            if (0 == si_member_value(si, "feed_by", value)) param["feed_by"] = value;
            if (0 == si_member_value(si, "filter", value)) param["filter"] = value;
        }
    }
    else {
        errorMsg = TRANSLATE_ME("Unexpected '%s' command (shall be 'to'/'from')", command.c_str()); // reason
        log_error("unexpected '%s' command  (shall be 'to'/'from')", command.c_str());
        return -1;
    }

    // trace param map
    for (std::map<std::string, std::string>::iterator it = param.begin(); it != param.end(); ++it) {
        log_trace("param['%s'] = '%s'", it->first.c_str(), it->second.c_str());
    }

    // request
    r = topology_location (param, result);
    if (r != 0) {
        errorMsg = param["error"]; // reason
        log_error("topology_location() failed, r: %d, command: %s, assetName: %s",
            r, command.c_str(), assetName.c_str());
        return -2;
    }

    // beautify result, optional
    if (beautify) {
        r = json_string_beautify(result);
        if (r != 0) {
            errorMsg = TRANSLATE_ME("JSON beautification failed"); // reason
            log_error("beautification failed, r: %d, result: \n%s", r, result.c_str());
            return -3;
        }
    }

    log_debug("topology_location() success, assetName: %s, result:\n%s",
        assetName.c_str(), result.c_str());

    return 0; // ok
}

// --------------------------------------------------------------------------
// Retrieve input power chain topology for a requested target asset
// Implementation of REST /api/v1/topology/input_power_chain (see RFC11)
// ASSETNAME is an assetID (datacenter)
// ERRORMSG set on failure (reason)
// On success, RESULT is valid (JSON payload)
// Returns 0 if success, else <0

int topology_input_powerchain_process (const std::string & assetName, std::string & result, std::string & errorMsg, bool beautify)
{
    result = "";

    std::map<std::string, std::string> param;
    param["id"] = assetName;

    // trace param map
    for (std::map<std::string, std::string>::iterator it = param.begin(); it != param.end(); ++it) {
        log_trace("param['%s'] = '%s'", it->first.c_str(), it->second.c_str());
    }

    // request
    int r = topology_input_powerchain (param, result);
    if (r != 0) {
        errorMsg = param["error"]; // reason
        log_error("topology_input_powerchain() failed, r: %d, assetName: %s",
            r, assetName.c_str());
        return -1;
    }

    // beautify result, optional
    if (beautify) {
        r = json_string_beautify(result);
        if (r != 0) {
            errorMsg = TRANSLATE_ME("JSON beautification failed"); // reason
            log_error("beautification failed, r: %d, result: \n%s", r, result.c_str());
            return -2;
        }
    }

    log_debug("topology_input_powerchain() success, assetName: %s, result:\n%s",
        assetName.c_str(), result.c_str());

    return 0; // ok
}

//  --------------------------------------------------------------------------
//  cxxtools, beautify S JSON string
//  S modified on success
//  Returns 0 if success, else <0

static int json_string_beautify (std::string & s)
{
    std::string beauty;
    cxxtools::SerializationInfo si;
    int r = string_to_si (s, si);
    if (r == 0) r = si_to_string (si, beauty, true);
    if (r == 0) s = beauty;
    return r;
}

//  --------------------------------------------------------------------------
//  cxxtools, dump SI to S string
//  Returns 0 if success, else <0

static int si_to_string (const cxxtools::SerializationInfo & si, std::string & s, bool beautify)
{
    try {
        s = "";
        std::ostringstream output;
        cxxtools::JsonSerializer js;
        js.beautify(beautify);
        js.begin(output);
        js.serialize(si);
        js.finish();
        s = output.str();
        return 0; // ok
    }
    catch (...) {
        log_error("Exception reached");
    }
    return -1;
}

//  --------------------------------------------------------------------------
//  cxxtools, set SI from S string
//  Returns 0 if success, else <0

static int string_to_si (const std::string & s, cxxtools::SerializationInfo & si)
{
    try {
        si.clear();
        std::istringstream input(s);
        cxxtools::JsonDeserializer deserializer(input);
        deserializer.deserialize(si);
        return 0; // ok
    }
    catch (...) {
        log_error("Exception reached");
    }
    return -1;
}

//  --------------------------------------------------------------------------
// json parser utility (get SI MEMBER string value)
// Returns 0 if success, else <0

static int si_member_value (const cxxtools::SerializationInfo & si, const std::string & member, std::string & value)
{
    value = "";
    const cxxtools::SerializationInfo *m = si.findMember(member);
    if (!m || m->isNull()) return -1;
    m->getValue(value);
    return 0; // ok
}
