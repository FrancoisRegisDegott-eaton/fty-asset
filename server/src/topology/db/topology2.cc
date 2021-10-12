/*  =========================================================================
    topology_db_topology2 - class description

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
    topology_db_topology2 -
@discuss
@end
*/

/**
 *  topology2.cc
 *
 *  next generation location_from DB layer
 *
 *  TODO:
 *  1. support for unlocated elements
 *  2. proper REST API reply
 *  3. ... pass the tests ...
 *
 */

#include "topology2.h"

#include <czmq.h>
#include <tntdb.h>
#include <algorithm>
#include <exception>
#include <tntdb/error.h>
#include <tntdb.h>
#include <sstream>
#include <cxxtools/split.h>
#include <cxxtools/jsondeserializer.h>
#include <cxxtools/jsonserializer.h>
#include <cxxtools/serializationinfo.h>

#include <fty_common.h>
#include <fty_common_macros.h>
#include <fty_common_db.h>

#define PARENT_LEVEL_COUNT 10

namespace persist {

static std::string
s_mkspc (int level);

class NodeMap {

    public:

        NodeMap ():
            _map {}
        {}

        void add (const std::string &name, const std::string &child) {
            add (name);
            _map [name].insert (child);
        }

        void add (const std::string &name) {
            if (_map.count (name) == 0) {
                std::set <std::string> s {};
                _map.emplace (std::make_pair (name, s));
            }
        }

        void print (const std::string &name, int level) {
            std::cout << s_mkspc (level) << name << ":" << std::endl;
            for (const auto &child: _map [name]) {
                print (child, level+1);
            }
        }

        void _feed_by (const std::string& name, std::set <std::string> &ret, std::set <std::string> &seen) {

            ret.insert (name);

            if (_map.count (name) == 0)
                return;

            auto ins = seen.insert (name);
            if (!ins.second) {
                std::string msg(TRANSLATE_ME("Power source loop detected:"));
                for (auto a : seen)
                    msg += " " + a;
                log_error("%s", msg.c_str());
                return;
            }
            for (const auto& kid: _map [name]) {
                _feed_by (kid, ret, seen);
            }
            seen.erase(ins.first);
        }

        // return a subtree - recursively
        std::set <std::string> feed_by (const std::string& name) {
            std::set <std::string> ret {}, seen {};
            _feed_by (name, ret, seen);
            return ret;
        }

        // return just kids
        std::set <std::string> at (const std::string& name) const {
            return _map.at (name);
        }

        bool has (const std::string& name) const {
            return _map.count (name) != 0;
        }

    private:
        std::map <std::string, std::set <std::string>> _map;
};

void operator<<= (cxxtools::SerializationInfo &si, const Item::Topology &topo)
{
    if (!topo.rooms.empty ())
        si.addMember("rooms") <<= topo.rooms;
    if (!topo.rows.empty ())
        si.addMember("rows") <<= topo.rows;
    if (!topo.racks.empty ())
        si.addMember("racks") <<= topo.racks;
    if (!topo.groups.empty ())
        si.addMember("groups") <<= topo.groups;
    if (!topo.devices.empty ())
        si.addMember("devices") <<= topo.devices;
}

void operator<<= (cxxtools::SerializationInfo &si, const Item &asset)
{
    si.addMember("name") <<= asset.name;
    si.addMember("id") <<= asset.id;
    si.addMember ("asset_order") <<= asset.asset_order;
    si.addMember("type") <<= asset.type;
    si.addMember("sub_type") <<= asset.subtype;
    if (!asset.contains.empty ())
    {
        si.addMember("contains") <<= asset.contains;
    }
}

// helper print function to be deleted
static const std::string
s_get (const tntdb::Row& row, const std::string& key) {
    try {
        return row.getString (key);
    }
    catch (const tntdb::NullValue &n) {
    }
    return "(null)";
}

static int
s_geti (const tntdb::Row& row, const std::string& key) {
    try {
        return row.getInt (key);
    }
    catch (const tntdb::NullValue &n) {
    }
    return -1;
}

static std::string
s_mkspc (int level) {
    std::string ret;
    if (level <= 0)
        return ret;
    for (int i = 0; i != level; i++) {
        ret.append ("  ");
    }
    return ret;
}

static void
s_topology2_devices_in_groups (
    tntdb::Connection& conn,
    Item &item)
{
    const std::string query = \
        " SELECT "
        "   el.name AS id, "
        "   el.id_type AS type, "
        "   el.id_subtype AS subtype, "
        "   ext.value AS name, "
        "   el2.name AS group_name, "
        "   torder.value AS asset_order "
        " FROM "
        "   t_bios_asset_group_relation rel "
        " JOIN t_bios_asset_element AS el "
        "   ON rel.id_asset_element=el.id_asset_element "
        " JOIN t_bios_asset_element AS el2 "
        "   ON rel.id_asset_group=el2.id_asset_element "
        " JOIN t_bios_asset_ext_attributes AS ext "
        "   ON el.id_asset_element=ext.id_asset_element "
        " LEFT JOIN t_bios_asset_ext_attributes AS torder ON (el.id_asset_element=torder.id_asset_element AND torder.keytag=\"name\") "
        " WHERE ext.keytag=\"name\" AND el2.name=:id "
        " ORDER BY "
        "   asset_order ASC ";

    tntdb::Statement st = conn.prepare (query);

    st.set ("id", item.id);

    for (const auto& row: st.select ()) {
        item.contains.push_back (Item {
            s_get (row, "id"),
            s_get (row, "name"),
            persist::subtypeid_to_subtype (static_cast<uint16_t>(s_geti (row, "subtype"))),
            persist::typeid_to_type (static_cast<uint16_t>(s_geti (row, "type")))
        });
    }
}

std::vector <Item>
topology2_groups (
    tntdb::Connection& conn,
    const std::string& id,
    bool recursive)
{
    const std::string query = \
        " SELECT "
        "   el.name AS id, "
        "   ext.value AS name "
        " FROM "
        "   t_bios_asset_group_relation AS rel "
        " JOIN "
        "   t_bios_asset_element AS el "
        " ON rel.id_asset_group=el.id_asset_element "
        " JOIN "
        "   t_bios_asset_element AS el2 "
        " ON rel.id_asset_element=el2.id_asset_element "
        " LEFT JOIN "
        "   t_bios_asset_ext_attributes AS ext "
        " ON ext.id_asset_element=el.id_asset_element "
        " WHERE el2.name=:id "
        "       AND keytag=\"name\" ";
    tntdb::Statement st = conn.prepare (query);
    st.set ("id", id);

    std::vector <Item> ret {};
    for (const auto& row: st.select ()) {

        Item item {
                s_get (row, "id"),
                s_get (row, "name"),
                "N_A",
                "group"
                };

        if (recursive)
            s_topology2_devices_in_groups (conn, item);
        ret.push_back (item);
    }
    return ret;
}

bool
is_power_device (tntdb::Connection &conn, std::string &asset_name)
{
    std::string query = "SELECT         "
                        "    id_subtype "
                        "FROM           "
                        "     t_bios_asset_element WHERE name=:asset_name";
    tntdb::Statement st = conn.prepare (query);
    try {
        tntdb::Result res = st.set("asset_name", asset_name).select ();
        std::string subtype;

        for (auto &row : res)
            subtype = s_get (row, "id_subtype");

        if (subtype == "1" ||
            subtype == "2" ||
            subtype == "3" ||
            subtype == "4" ||
            subtype == "6" ||
            subtype == "7"
        )
            return true;
        else
            return false;
    }
    catch (const std::exception &e) {
        log_error ("Exception caught: is_power_device %s", e.what ());
    }
    return false;
}

//  return a set of devices feeded by feed_by
//
//  feed_by - return devices feed by given iname
//
//  return tntdb::Result
//
std::set <std::string>
topology2_feed_by (
    tntdb::Connection& conn,
    const std::string& feed_by)
{
    std::string query = "SELECT src_name, dest_name FROM v_bios_asset_link_topology WHERE id_asset_link_type = 1";
    tntdb::Statement st = conn.prepare (query);

    NodeMap nm{};

    for (const auto& row: st.select ()) {

        std::string name = s_get (row, "src_name");

        std::string kid = s_get (row, "dest_name");

        nm.add (name, kid);
    }

    return nm.feed_by (feed_by);
}

//  return a topology
//
//  from    - iname of asset where topology starts
//  filter  - (datacenter,row,rack,room,device) - show only selected types
//  recursive - true|false
//  feed_by - additional filtering - only devices feed by given iname
//
//  return tntdb::Result
//

tntdb::Result
topology2_from (
    tntdb::Connection& conn,
    const std::string& from)
{

    // TODO: db error handling
    std::string query = \
        " SELECT "
        "    t1.id AS DBID1,    "
        "    t2.id AS DBID2,    "
        "    t3.id AS DBID3,    "
        "    t4.id AS DBID4,    "
        "    t5.id AS DBID5,    "
        "    t6.id AS DBID6,    "
        "    t7.id AS DBID7,    "
        "    t8.id AS DBID8,    "
        "    t9.id AS DBID9,    "
        "    t10.id AS DBID10,  "
        "    t11.id AS DBID11,  "
        "    t1.name AS ID1,    "
        "    t2.name AS ID2,    "
        "    t3.name AS ID3,    "
        "    t4.name AS ID4,    "
        "    t5.name AS ID5,    "
        "    t6.name AS ID6,    "
        "    t7.name AS ID7,    "
        "    t8.name AS ID8,    "
        "    t9.name AS ID9,    "
        "    t10.name AS ID10,  "
        "    t11.name AS ID11,  "
        "    t1.id_type AS TYPEID1,     "
        "    t2.id_type AS TYPEID2,     "
        "    t3.id_type AS TYPEID3,     "
        "    t4.id_type AS TYPEID4,     "
        "    t5.id_type AS TYPEID5,     "
        "    t6.id_type AS TYPEID6,     "
        "    t7.id_type AS TYPEID7,     "
        "    t8.id_type AS TYPEID8,     "
        "    t9.id_type AS TYPEID9,     "
        "    t10.id_type AS TYPEID10,   "
        "    t11.id_type AS TYPEID11,   "
        "    t1.id_subtype AS SUBTYPEID1,   "
        "    t2.id_subtype AS SUBTYPEID2,   "
        "    t3.id_subtype AS SUBTYPEID3,   "
        "    t4.id_subtype AS SUBTYPEID4,   "
        "    t5.id_subtype AS SUBTYPEID5,   "
        "    t6.id_subtype AS SUBTYPEID6,   "
        "    t7.id_subtype AS SUBTYPEID7,   "
        "    t8.id_subtype AS SUBTYPEID8,   "
        "    t9.id_subtype AS SUBTYPEID9,   "
        "    t10.id_subtype AS SUBTYPEID10, "
        "    t11.id_subtype AS SUBTYPEID11, "
        "    t1ext.value AS ASSET_ORDER1,   "
        "    t2ext.value AS ASSET_ORDER2,   "
        "    t3ext.value AS ASSET_ORDER3,   "
        "    t4ext.value AS ASSET_ORDER4,   "
        "    t5ext.value AS ASSET_ORDER5,   "
        "    t6ext.value AS ASSET_ORDER6,   "
        "    t7ext.value AS ASSET_ORDER7,   "
        "    t8ext.value AS ASSET_ORDER8,   "
        "    t9ext.value AS ASSET_ORDER9,   "
        "    t10ext.value AS ASSET_ORDER10, "
        "    t11ext.value AS ASSET_ORDER11, "
        "    t1name.value AS NAME1,     "
        "    t2name.value AS NAME2,     "
        "    t3name.value AS NAME3,     "
        "    t4name.value AS NAME4,     "
        "    t5name.value AS NAME5,     "
        "    t6name.value AS NAME6,     "
        "    t7name.value AS NAME7,     "
        "    t8name.value AS NAME8,     "
        "    t9name.value AS NAME9,     "
        "    t10name.value AS NAME10,   "
        "    t11name.value AS NAME11    "
        "  FROM v_bios_asset_element AS t1  "
        "    LEFT JOIN v_bios_asset_element AS t2 ON t2.id_parent = t1.id       "
        "    LEFT JOIN v_bios_asset_element AS t3 ON t3.id_parent = t2.id       "
        "    LEFT JOIN v_bios_asset_element AS t4 ON t4.id_parent = t3.id       "
        "    LEFT JOIN v_bios_asset_element AS t5 ON t5.id_parent = t4.id       "
        "    LEFT JOIN v_bios_asset_element AS t6 ON t6.id_parent = t5.id       "
        "    LEFT JOIN v_bios_asset_element AS t7 ON t7.id_parent = t6.id       "
        "    LEFT JOIN v_bios_asset_element AS t8 ON t8.id_parent = t7.id       "
        "    LEFT JOIN v_bios_asset_element AS t9 ON t9.id_parent = t8.id       "
        "    LEFT JOIN v_bios_asset_element AS t10 ON t10.id_parent = t9.id     "
        "    LEFT JOIN v_bios_asset_element AS t11 ON t11.id_parent = t10.id    "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t1ext ON (t1.id = t1ext.id_asset_element AND t1ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t2ext ON (t2.id = t2ext.id_asset_element AND t2ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t3ext ON (t3.id = t3ext.id_asset_element AND t3ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t4ext ON (t4.id = t4ext.id_asset_element AND t4ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t5ext ON (t5.id = t5ext.id_asset_element AND t5ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t6ext ON (t6.id = t6ext.id_asset_element AND t6ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t7ext ON (t7.id = t7ext.id_asset_element AND t7ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t8ext ON (t8.id = t8ext.id_asset_element AND t8ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t9ext ON (t9.id = t9ext.id_asset_element AND t9ext.keytag=\"asset_order\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t10ext ON (t10.id = t10ext.id_asset_element AND t10ext.keytag=\"asset_order\")    "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t11ext ON (t11.id = t11ext.id_asset_element AND t11ext.keytag=\"asset_order\")    "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t1name ON (t1.id = t1name.id_asset_element AND t1name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t2name ON (t2.id = t2name.id_asset_element AND t2name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t3name ON (t3.id = t3name.id_asset_element AND t3name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t4name ON (t4.id = t4name.id_asset_element AND t4name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t5name ON (t5.id = t5name.id_asset_element AND t5name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t6name ON (t6.id = t6name.id_asset_element AND t6name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t7name ON (t7.id = t7name.id_asset_element AND t7name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t8name ON (t8.id = t8name.id_asset_element AND t8name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t9name ON (t9.id = t9name.id_asset_element AND t9name.keytag=\"name\")        "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t10name ON (t10.id = t10name.id_asset_element AND t10name.keytag=\"name\")    "
        "    LEFT JOIN t_bios_asset_ext_attributes AS t11name ON (t11.id = t11name.id_asset_element AND t11name.keytag=\"name\")    "
        "    INNER JOIN t_bios_asset_device_type AS v1 ON (v1.id_asset_device_type = t1.id_subtype) "
        "  WHERE t1.name=:from ";

    tntdb::Statement st = conn.prepare (query);

    st.set ("from", from);
    return st.select ();
}

static int
s_filter_type (const std::string &_filter)
{
    if (!_filter.empty ()) {
        std::string filter = _filter;
        filter.pop_back ();
        return persist::type_to_typeid (filter);
    }
    return -1;
}

static bool
s_should_filter (int filter_type, int type)
{
    return filter_type != -1 && type != filter_type;
}

static bool
fctOrderByName (const Item &i1, const Item &i2)
{
    return (i1.name < i2.name);
}

// MVY: TODO - it turns out that topology call is way more simpler than this
//             therefor simply change SQL SELECT to get devices with id_parent==id (fom)
void
topology2_from_json (
    std::ostream &out,
    tntdb::Result &res,
    const std::string &from,
    const std::string &filter,
    const std::set <std::string> &feeded_by,
    const std::vector <Item> &groups)
{
    Item item_from {};
    Item::Topology topo {};

    std::set <std::string> processed {};

    int filter_type = s_filter_type (filter);

    for (const auto& row: res) {
        for (int i = 1; i != 3; i++) {

            std::string idx = std::to_string (i);
            std::string ID {"ID"}; ID.append (idx);
            std::string TYPE {"TYPEID"}; TYPE.append (idx);
            std::string SUBTYPE {"SUBTYPEID"}; SUBTYPE.append (idx);
            std::string NAME {"NAME"}; NAME.append (idx);
            std::string ASSET_ORDER {"ASSET_ORDER"}; ASSET_ORDER.append (idx);

            // feed_by filtering
            std::string id = s_get (row, ID);

            if (id == from) {
                item_from = Item {
                    id,
                    s_get (row, NAME),
                    persist::subtypeid_to_subtype (static_cast<uint16_t>(s_geti (row, SUBTYPE))),
                    persist::typeid_to_type (static_cast<uint16_t>(s_geti (row, TYPE)))};
                continue;
            }

            if (!feeded_by.empty () && feeded_by.count (id) == 0)
                continue;

            // filter - type filtering
            int type = s_geti (row, TYPE);
            if (s_should_filter (filter_type, type))
                continue;

            if (processed.count (id) != 0 || id == "(null)")
                continue;

            Item item {
                id,
                s_get (row, NAME),
                persist::subtypeid_to_subtype (static_cast<uint16_t>(s_geti (row, SUBTYPE))),
                persist::typeid_to_type (static_cast<uint16_t>(s_geti (row, TYPE)))};
            item.asset_order = s_geti (row, ASSET_ORDER);

            if (item.asset_order < 0) {
                item.asset_order = 0;
            }
            topo.push_back (item);

            processed.emplace (id);
        }
        topo.sort (fctOrderByName);
        topo.groups.insert (topo.groups.end (), groups.begin (), groups.end ());
    }

    cxxtools::JsonSerializer serializer (out);
    serializer.beautify (false);
    serializer.inputUtf8(true);
    item_from.contains = topo;
    serializer.serialize(item_from).finish();
}

static void
s_topo_recursive (
    Item::Topology &topo,
    std::set <std::string> from,
    const NodeMap &nm,
    std::map <std::string, Item> &im)
{
    if (from.empty ())
        return;

    for (const std::string &id : from) {
        Item it;
        try {
            it = im.at (id);
        } catch (std::exception &e) {
            continue;
        }
        std::set <std::string> kids;

        // collect all kids
        if (nm.has (id) && ! nm.at (id).empty()) {
            for (const auto &kid: nm.at (id)) {
                kids.insert (kid);
            }
        }

        // add kids recursivelly to topology
        if (! kids.empty ()) {
            it.contains = Item::Topology {};
            s_topo_recursive (it.contains, kids, nm, im);
        }
        topo.push_back (it);
        topo.sort (fctOrderByName);
    }
}

static bool
s_should_filter_recursive (int query_type, int asset_type)
{
    // do not filter if no filter= has been specified in query
    if (query_type == -1)
        return false;

    // if we're asking about groups, return groups only
    if (query_type == persist::asset_type::GROUP)
        return asset_type != persist::asset_type::GROUP;

    // else filter all types with number lower than asked for
    return query_type < asset_type || asset_type == persist::asset_type::GROUP;
}

void
topology2_from_json_recursive (
    std::ostream &out,
    tntdb::Connection &conn,
    tntdb::Result &res,
    const std::string &from,
    const std::string &filter,
    const std::set <std::string> &feeded_by,
    const std::vector <Item> &groups)
{
    NodeMap nm {};
    // build the topology using NodeMap put data to map string->Item
    for (const auto& row: res) {
        for (int i = 1; i != PARENT_LEVEL_COUNT; i++) {
            std::string name = s_get (row, "ID" + std::to_string (i));
            std::string kid = s_get (row, "ID" + std::to_string (i+1));

            if (name != "(null)") {
                if (kid != "(null)")
                    nm.add (name, kid);
                else
                    nm.add (name);
            }
        }
    }

    int query_type = s_filter_type (filter);

    // create a map id -> Item
    std::set <std::string> processed {};
    std::map <std::string, Item> im {};
    std::string from_type;
    std::string from_subtype;
    Item it2;
    for (const auto& row: res) {

        for (int i = 1; i != PARENT_LEVEL_COUNT+1; i++) {

            std::string idx = std::to_string (i);
            std::string ID {"ID"}; ID.append (idx);
            std::string TYPE {"TYPEID"}; TYPE.append (idx);
            std::string SUBTYPE {"SUBTYPEID"}; SUBTYPE.append (idx);
            std::string NAME {"NAME"}; NAME.append (idx);
            std::string ASSET_ORDER {"ASSET_ORDER"}; ASSET_ORDER.append (idx);

            std::string id = s_get (row, ID);

            if (!id.compare (from)) {

                from_type = persist::typeid_to_type (static_cast<uint16_t>(s_geti (row, TYPE)));
                from_subtype = persist::subtypeid_to_subtype (static_cast<uint16_t>(s_geti (row, SUBTYPE)));

                it2.id = from;
                it2.name = s_get (row, NAME),
                it2.subtype =  from_subtype;
                it2.type =  from_type;
                it2.asset_order = 0;
            }

            // feed_by filtering - for devices only
            int type = s_geti (row, TYPE);
            if (type == persist::asset_type::DEVICE
            && (!feeded_by.empty () && feeded_by.count (id) == 0))
                continue;

            // filter - type filtering
            if (s_should_filter_recursive (query_type, type))
                continue;

            if (processed.count (id) != 0 || id == "(null)")
                continue;

            Item it {
                id,
                    s_get (row, NAME),
                    persist::subtypeid_to_subtype (static_cast<uint16_t>(s_geti (row, SUBTYPE))),
                    persist::typeid_to_type (static_cast<uint16_t>(s_geti (row, TYPE)))};
            it.asset_order = s_geti (row, ASSET_ORDER);
            if (it.asset_order < 0)
                it.asset_order = 0;

            if (s_geti (row, TYPE) == persist::asset_type::GROUP)
                s_topology2_devices_in_groups (conn, it);

            im.insert (std::make_pair (id, it));
            processed.emplace (id);
        }
    }

    Item::Topology topo {};
    s_topo_recursive (
        topo,
        nm.at (from),
        nm,
        im);

    if (query_type == persist::asset_type::GROUP)
        topo.groups.insert (topo.groups.end (), groups.begin (), groups.end ());

    cxxtools::JsonSerializer serializer (out);
    serializer.beautify (false);
    serializer.inputUtf8(true);
    it2.contains = topo;
    serializer.serialize(it2).finish();
}

}// namespace persist


