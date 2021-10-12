/*  =========================================================================
    topology_topology_input_powerchain - class description

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
    topology_topology_input_powerchain -
@discuss
@end
*/

#include "topology_input_powerchain.h"
#include <fty_common.h>

////////////////////////////////////////////////////////////////////////
// copied from fty-rest
//      see src/wab/tntnet.xml
//      src/web/src/input_power_chain.ecpp
// C++ transformed
// see also code in src/topology/
// implementation of REST /api/v1/topology/input_power_chain (see RFC11)
////////////////////////////////////////////////////////////////////////

/*!
 * \file input_power_chain.ecpp
 * \author Barbora Stepankova <BarboraStepankova@Eaton.com>
 * \brief Returns input power chain.
 */

#include <string>
#include <iostream>

#include <cxxtools/jsonserializer.h>

#include "fty_log.h"
#include "utilspp.h"

#include <fty_common_db.h>
#include <fty_common.h>
#include "assettopology.h"


// struct for "devices" array
struct Array_devices
{
    std::string name;
    std::string id;
    std::string sub_type;
};

// struct for "powerchains" array
struct Array_power_chain
{
    std::string src_id;
    std::string dst_id;
    std::string src_socket;
    std::string dst_socket;
};

// main json structure for json response
struct Topology
{
    std::vector <Array_devices> devices;
    std::vector <Array_power_chain> powerchains;
};

// that's how main structure is serialized
static
void operator<<= (cxxtools::SerializationInfo& si, const Topology& input_power)
{
    si.addMember("devices") <<= input_power.devices;
    si.addMember("powerchains") <<= input_power.powerchains;
}

// that's how "devices" array is serialized
static
void operator<<= (cxxtools::SerializationInfo& si, const Array_devices& array_devices)
{

    si.addMember("name") <<= array_devices.name;
    si.addMember("id") <<= array_devices.id;
    si.addMember("sub_type") <<= utils::strip (array_devices.sub_type);
}

// that's how "powerchains" array is serialized
static
void operator<<= (cxxtools::SerializationInfo& si, const Array_power_chain& array_power_chain)
{
    si.addMember("src-id") <<= array_power_chain.src_id;
    si.addMember("dst-id") <<= array_power_chain.dst_id;

    if (array_power_chain.src_socket != "")
       si.addMember("src-socket") <<= array_power_chain.src_socket;
    if (array_power_chain.dst_socket != "")
        si.addMember("dst-socket") <<= array_power_chain.dst_socket;
}

static int
s_fill_array_devices (const std::map <std::string,
    std::pair <std::string, std::string>> map_devices,
    std::vector <Array_devices>& devices_vector)
{
    Array_devices array_devices;
    std::map <std::string, std::pair <std::string, std::string>> _map_devices = map_devices;
    for (const auto& device : _map_devices)
    {
        array_devices.id = device.second.first;
        std::pair<std::string, std::string> device_names = DBAssets::id_to_name_ext_name (static_cast<uint32_t>(atoi (device.first.c_str())));
        if (device_names.first.empty () && device_names.second.empty ())
            return -1;
        array_devices.name = device_names.second;

        array_devices.sub_type = device.second.second;

        devices_vector.push_back (array_devices);
    }
    return 0; // ok
}

static int
s_fill_array_powerchains (
    std::vector <std::tuple
     <std::string,
      std::string,
      std::string,
      std::string>> vector_powerchains,
    std::vector <Array_power_chain>& powerchains_vector)
{
    Array_power_chain array_powerchains;
    for (const auto& chain : vector_powerchains)
    {
        array_powerchains.src_socket = std::get <3> (chain).c_str();
        array_powerchains.dst_socket = std::get <1> (chain).c_str();

        array_powerchains.src_id = DBAssets::id_to_name_ext_name (static_cast<uint32_t>(atoi (std::get <2> (chain).c_str()))).first;
        array_powerchains.dst_id = DBAssets::id_to_name_ext_name (static_cast<uint32_t>(atoi (std::get <0> (chain).c_str ()))).first;

        std::pair<std::string, std::string> src_names = DBAssets::id_to_name_ext_name (static_cast<uint32_t>(atoi (std::get <2> (chain).c_str())));
        if (src_names.first.empty () && src_names.second.empty ())
            return -1;
        array_powerchains.src_id = src_names.first;

        std::pair<std::string, std::string> dst_names = DBAssets::id_to_name_ext_name (static_cast<uint32_t>(atoi (std::get <0> (chain).c_str ())));
        if (dst_names.first.empty () && dst_names.second.empty ())
            return -2;
        array_powerchains.dst_id = dst_names.first;

        powerchains_vector.push_back (array_powerchains);
    }
    return 0; // ok
}

//  topology_input_powerchain main entry
//  PARAM map keys in 'id'
//  source: fty-rest /api/v1/topology/input_power_chain REST api
//      src/web/tntnet.xml, src/web/src/input_power_chain.ecpp
//  returns 0 if success (json payload is valid), else <0

int topology_input_powerchain (std::map<std::string, std::string> & param, std::string & json)
{
    json = "";

    // id of datacenter retrieved from url
    std::string dc_id = param["id"];
    if (dc_id.empty ()) {
        //http_die ("request-param-bad", dc_id.c_str ());
        log_error ("request-param-bad 'id' is empty");
        param["error"] = TRANSLATE_ME("Asset not defined");
        return -1;
    }

    int64_t dbid = DBAssets::name_to_asset_id (dc_id);
    if (dbid == -1) {
        //http_die ("element-not-found", "dc_id ", dc_id.c_str ());
        log_error ("element-not-found (dc_id: '%s')", dc_id.c_str ());
        param["error"] = TRANSLATE_ME("Asset not found (%s)", dc_id.c_str ());
        return -2;
    }
    if (dbid == -2) {
        //http_die ("internal-error", "Connecting to database failed.");
        log_error ("internal-error, Connecting to database failed.");
        param["error"] = TRANSLATE_ME("Connection to database failed");
        return -3;
    }

    // data for powerchains -- dst-id, dst-socket, src-id, src-socket
    std::vector <std::tuple <std::string, std::string, std::string, std::string>> powerchains_data;
    // device_id -- <device name, device subtype>
    std::map <std::string, std::pair <std::string, std::string>> devices_data;

    int r = input_power_group_response (DBConn::url, static_cast<uint32_t> (dbid), devices_data, powerchains_data);
    if (r == -1) {
        //http_die ("internal-error", "input_power_group_response");
        log_error ("internal-error, input_power_group_response r = %d", r);
        param["error"] = TRANSLATE_ME("Get input power group failed");
        return -4;
    }

    if (devices_data.size () == 0) { log_trace ("devices_data.size () == 0"); }
    if (powerchains_data.size () == 0) { log_trace ("powerchains_data.size () == 0"); }

    std::vector <Array_devices> devices_vector;
    r = s_fill_array_devices (devices_data, devices_vector);
    if (r != 0) {
        //http_die ("internal-error", "Database failure");
        log_error ("internal-error, Database failure (r: %d)", r);
        param["error"] = TRANSLATE_ME("Database access failed");
        return -5;
    }

    std::vector <Array_power_chain> powerchains_vector;
    r = s_fill_array_powerchains (powerchains_data, powerchains_vector);
    if (r != 0) {
        //http_die ("internal-error", "Database failure");
        log_error ("internal-error, Database failure (r: %d)", r);
        param["error"] = TRANSLATE_ME("Database access failed");
        return -6;
    }

    // aggregate
    Topology topo;
    topo.devices = std::move (devices_vector);
    topo.powerchains = std::move (powerchains_vector);

    // serialize topo (json)
    try {
        std::ostringstream out;
        cxxtools::JsonSerializer serializer (out);
        serializer.inputUtf8(true);
        serializer.serialize(topo).finish();
        json = out.str();
    }
    catch (...) {
        log_error ("internal-error, json serialization failed (raise exception)");
        param["error"] = TRANSLATE_ME("JSON serialization failed");
        return -7;
    }

    return 0; // ok
}

