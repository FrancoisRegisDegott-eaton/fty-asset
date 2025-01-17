/*
 *
 * Copyright (C) 2015 - 2018 Eaton
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

#include "asset/csv.h"
#include <algorithm>
#include <cxxtools/csvdeserializer.h>
#include <cxxtools/serializationinfo.h>
#include <fty_common.h>
#include <fty_common_macros.h>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace fty::asset {

template <typename T, typename = void>
struct HasRead : std::false_type
{
};

template <typename T>
struct HasRead<T, decltype(std::declval<T>().read, void())> : std::true_type
{
};

/* Workaround for a fact a) std::transform to do a strip and lower is weird, b) it breaks the map somehow*/
static const std::string _ci_strip(const std::string& str)
{
    std::ostringstream b;

    for (const char c : str) {
        // allowed chars [a-zA-Z0-9_\.]
        if (::isalnum(c) || c == '_' || c == '.')
            b << static_cast<char>(::tolower(c));
    }

    return b.str();
}

void CsvMap::deserialize()
{

    if (_data.size() == 0) {
        throw std::invalid_argument(TRANSLATE_ME("Can't process empty data set"));
    }

    size_t i = 0;
    for (const std::string& title_name : _data[0]) {
        std::string title = _ci_strip(title_name);
        if (_title_to_index.count(title) == 1) {
            /*std::string msg = TRANSLATE_ME("duplicate title name '%s'", title.c_str());
            throw std::invalid_argument(msg);*/
            log_warning("Duplicate title name '%s', we will still use the previous on.", title.c_str());
        } else {
            _title_to_index.emplace(title, i);            
        }


        i++;
    }
}

const std::string& CsvMap::get(size_t row_i, const std::string& title_name) const
{

    if (row_i >= _data.size()) {
        std::string msg = TRANSLATE_ME("row_index %zu was out of range %zu", row_i, _data.size());
        throw std::out_of_range(msg);
    }

    std::string title = _ci_strip(title_name);

    if (_title_to_index.count(title) == 0) {
        std::string msg = TRANSLATE_ME("title name '%s' not found", title.c_str());
        throw std::out_of_range{msg};
    }

    size_t col_i = _title_to_index.at(title);
    if (col_i >= _data[row_i].size()) {
        const char* err = "On line %zu: requested column %s (index %zu) where maximum is %zu";
        throw std::out_of_range(TRANSLATE_ME(err, row_i + 1, title_name.c_str(), col_i + 1, _data[row_i].size()));
    }
    return _data[row_i][col_i];
}

std::string CsvMap::get_strip(size_t row_i, const std::string& title_name) const
{
    return _ci_strip(get(row_i, title_name));
}

bool CsvMap::hasTitle(const std::string& title_name) const
{
    std::string title = _ci_strip(title_name);
    return (_title_to_index.count(title) == 1);
}

std::set<std::string> CsvMap::getTitles() const
{
    std::set<std::string> ret{};
    for (auto i : _title_to_index) {
        ret.emplace(i.first);
    }
    return ret;
}

std::string CsvMap::getCreateUser() const
{
    return _create_user;
}
std::string CsvMap::getUpdateUser() const
{
    return _update_user;
}
std::string CsvMap::getUpdateTs() const
{
    return _update_ts;
}
uint32_t CsvMap::getCreateMode() const
{
    return _create_mode;
}

void CsvMap::setCreateUser(std::string user)
{
    _create_user = user;
}
void CsvMap::setUpdateUser(std::string user)
{
    _update_user = user;
}
void CsvMap::setUpdateTs(std::string timestamp)
{
    _update_ts = timestamp;
}
void CsvMap::setCreateMode(uint32_t mode)
{
    _create_mode = mode;
}

// TODO: does not belongs to csv, move somewhere else
void skip_utf8_BOM(std::istream& i)
{
    int c1, c2, c3;
    c1 = i.get();
    c2 = i.get();
    c3 = i.get();

    if (c1 == 0xef && c2 == 0xbb && c3 == 0xbf)
        return;

    i.putback(char(c3));
    i.putback(char(c2));
    i.putback(char(c1));
}

char findDelimiter(std::istream& i, std::size_t max_pos)
{
    for (std::size_t pos = 0; i.good() && !i.eof() && pos != max_pos; pos++) {
        i.seekg(long(pos));
        char ret = char(i.peek());
        if (ret == ',' || ret == ';' || ret == '\t') {
            i.seekg(0);
            return ret;
        }
    }
    i.seekg(0);
    return '\x0';
}

bool hasApostrof(std::istream& i)
{
    for (std::size_t pos = 0; i.good() && !i.eof(); pos++) {
        i.seekg(long(pos));
        char ret = char(i.peek());
        if (ret == '\'') {
            i.seekg(0);
            return true;
        }
    }
    i.seekg(0);
    return false;
}

CsvMap CsvMap_from_istream(std::istream& in)
{
    cxxtools::CsvDeserializer             deserializer(in);
    std::vector<std::vector<std::string>> data;
    char                                  delimiter = findDelimiter(in);

    if (delimiter == '\x0') {
        std::string msg = TRANSLATE_ME("Cannot detect the delimiter, use comma (,) semicolon (;) or tabulator");
        log_error("%s\n", msg.c_str());
        LOG_END;
        throw std::invalid_argument(msg);
    }
    log_debug("Using delimiter '%c'", delimiter);
    deserializer.delimiter(delimiter);
    deserializer.readTitle(false);
    deserializer.deserialize(data);
    CsvMap cm{data};
    cm.deserialize();
    return cm;
}


static void process_powers_key(
    const cxxtools::SerializationInfo& powers_si, std::vector<std::vector<std::string>>& data)
{
    if (powers_si.category() != cxxtools::SerializationInfo::Array) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'powers' should be an array"));
    }
    // we need a counter for fields
    int i = 1;
    for (const auto& oneElement : powers_si) { // iterate through the array
        // src_name is mandatory
        if (oneElement.findMember("src_name") == nullptr) {
            throw std::invalid_argument(TRANSLATE_ME("Key 'src_name' in the key 'powers' is mandatory"));
        }

        std::string src_name;
        oneElement.getMember("src_name") >>= src_name;
        data[0].push_back("power_source." + std::to_string(i));
        data[1].push_back(src_name);
        // src_outlet is optimal
        if (oneElement.findMember("src_socket") != nullptr) {
            std::string src_socket{};
            oneElement.getMember("src_socket") >>= src_socket;
            data[0].push_back("power_plug_src." + std::to_string(i));
            data[1].push_back(src_socket);
        }
        // dest_outlet is optional
        if (oneElement.findMember("dest_socket") != nullptr) {
            std::string dest_socket;
            oneElement.getMember("dest_socket") >>= dest_socket;
            data[0].push_back("power_input." + std::to_string(i));
            data[1].push_back(dest_socket);
        }
        // src_id is there, but here it is ignored
        // because id and name can be in the conflict.
        // src_name is mutable, but src_id is just an informational field
        i++;
    }
}


static void process_groups_key(
    const cxxtools::SerializationInfo& groups_si, std::vector<std::vector<std::string>>& data)
{
    if (groups_si.category() != cxxtools::SerializationInfo::Array) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'groups' should be an array"));
    }
    // we need a counter for fields
    int i = 1;
    for (const auto& oneElement : groups_si) { // iterate through the array
        // id is just an informational field, ignore it here
        // name is mandatory
        if (oneElement.findMember("name") == nullptr) {
            throw std::invalid_argument(TRANSLATE_ME("Key 'name' in the key 'groups' is mandatory"));
        } else {
            std::string group_name;
            oneElement.getMember("name") >>= group_name;
            data[0].push_back("group." + std::to_string(i));
            data[1].push_back(group_name);
        }
        i++;
    }
}


static void process_ips_key(const cxxtools::SerializationInfo& si, std::vector<std::vector<std::string>>& data)
{
    LOG_START;
    if (si.category() != cxxtools::SerializationInfo::Array) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'ips' should be an array"));
    }
    // we need a counter for fields
    int i = 1;
    for (const auto& oneElement : si) { // iterate through the array
        std::string name{"ip." + std::to_string(i)};
        i++;

        // ips VS ip.X: avoid duplicate
        if (std::find(data[0].begin(), data[0].end(), name) != data[0].end()) {
            continue;
        }

        std::string value;
        oneElement.getValue(value);
        data[0].push_back(name);
        data[1].push_back(value);
    }
}


static void process_macs_key(const cxxtools::SerializationInfo& si, std::vector<std::vector<std::string>>& data)
{
    LOG_START;
    if (si.category() != cxxtools::SerializationInfo::Array) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'macs' should be an array"));
    }
    // we need a counter for fields
    int i = 1;
    for (const auto& oneElement : si) { // iterate through the array
        std::string value;
        oneElement.getValue(value);
        data[0].push_back("mac." + std::to_string(i));
        data[1].push_back(value);
        i++;
    }
}


static void process_hostnames_key(const cxxtools::SerializationInfo& si, std::vector<std::vector<std::string>>& data)
{
    LOG_START;
    if (si.category() != cxxtools::SerializationInfo::Array) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'hostnames' should be an array"));
    }
    // we need a counter for fields
    int i = 1;
    for (const auto& oneElement : si) { // iterate through the array
        std::string value;
        oneElement.getValue(value);
        data[0].push_back("hostname." + std::to_string(i));
        data[1].push_back(value);
        i++;
    }
}


static void process_fqdns_key(const cxxtools::SerializationInfo& si, std::vector<std::vector<std::string>>& data)
{
    LOG_START;
    if (si.category() != cxxtools::SerializationInfo::Array) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'fqdns' should be an array"));
    }
    // we need a counter for fields
    int i = 1;
    for (const auto& oneElement : si) { // iterate through the array
        std::string value;
        oneElement.getValue(value);
        data[0].push_back("fqdn." + std::to_string(i));
        data[1].push_back(value);
        i++;
    }
}

static void process_oneOutlet(const cxxtools::SerializationInfo& outlet_si, std::vector<std::vector<std::string>>& data)
{
    if (outlet_si.category() != cxxtools::SerializationInfo::Array) {
        std::string msg = TRANSLATE_ME("Key '%s' should be an array", outlet_si.name().c_str());
        throw std::invalid_argument(msg);
    }
    std::string name;
    std::string value;
    bool        isReadOnly = false;
    for (const auto& oneOutletAttr : outlet_si) { // iterate through the outletAttributes
        try {
            oneOutletAttr.getMember("name").getValue(name);
            oneOutletAttr.getMember("value").getValue(value);
            oneOutletAttr.getMember("read_only").getValue(isReadOnly);
        } catch (...) {
            throw std::invalid_argument(TRANSLATE_ME("In outlet object key 'name/value/read_only' is missing"));
        }
        data[0].push_back("outlet." + outlet_si.name() + "." + name);
        data[1].push_back(value);
    }
}


static void process_outlets_key(
    const cxxtools::SerializationInfo& outlets_si, std::vector<std::vector<std::string>>& data)
{
    if (outlets_si.category() != cxxtools::SerializationInfo::Object) {
        throw std::invalid_argument(TRANSLATE_ME("Key 'outlets' should be an object"));
    }
    for (const auto& oneOutlet : outlets_si) { // iterate through the object
        process_oneOutlet(oneOutlet, data);
    }
}


// forward declaration
static void s_read_si(const cxxtools::SerializationInfo& si, std::vector<std::vector<std::string>>& data);

static void process_ext_key(const cxxtools::SerializationInfo& ext_si, std::vector<std::vector<std::string>>& data)
{
    if (ext_si.category() == cxxtools::SerializationInfo::Array) {
        log_debug("it is GET format");
        // this information in GET format
        for (const auto& oneAttrEl : ext_si) { // iterate through the array
            if (oneAttrEl.memberCount() != 2) {
                throw std::invalid_argument(TRANSLATE_ME("Expected two properties per each ext attribute"));
            }
            // ASSUMPTION: oneAttr has 2 fields:
            // "read_only" - this is an information only
            // "some_unknown_name"
            for (unsigned int i = 0; i < 2; ++i) {
                auto& oneAttr = oneAttrEl.getMember(i);
                auto& name    = oneAttr.name();
                if (name == "read_only") {
                    continue;
                }

                // ips VS ip.X: avoid duplicate
                if ((name.find("ip.") == 0) && (std::find(data[0].begin(), data[0].end(), name) != data[0].end())) {
                    break;
                }

                cxxtools::String value;
                oneAttr.getValue(value);
                data[0].push_back(name);

                data[1].push_back(cxxtools::Utf8Codec::encode(value));
            }
        }
    } else if (ext_si.category() == cxxtools::SerializationInfo::Object) {
        // this information in PUT POST format
        log_debug("it is PUT/POST format");
        s_read_si(ext_si, data);
    } else {
        throw std::invalid_argument(TRANSLATE_ME("Key 'ext' should be an Array or Object"));
    }
}

static void s_read_si(const cxxtools::SerializationInfo& si, std::vector<std::vector<std::string>>& data)
{
    if (data.size() != 2) {
        throw std::invalid_argument(TRANSLATE_ME("Expected two items in array, got %zu", data.size()));
    }

    for (auto it = si.begin(); it != si.end(); ++it) {
        std::string name = it->name();
        if (name == "ext") {
            process_ext_key(si.getMember("ext"), data);
            continue;
        }
        // these fields are just for the information in the REPRESENTATION
        // may be we can remove them earlier, but for now it is HERE
        // TODO: BIOS-1428
        if (name == "location_id") {
            continue;
        }
        if (name == "location_uri") {
            continue;
        }
        if (name == "power_devices_in_uri") {
            continue;
        }
        // BIOS-1101
        if (name == "parents") {
            continue;
        }

        if (name == "powers") {
            process_powers_key(si.getMember("powers"), data);
            continue;
        }
        if (name == "groups") {
            process_groups_key(si.getMember("groups"), data);
            continue;
        }
        if (name == "outlets") {
            process_outlets_key(si.getMember("outlets"), data);
            continue;
        }
        if (name == "ips") {
            process_ips_key(si.getMember("ips"), data);
            continue;
        }
        if (name == "macs") {
            process_macs_key(si.getMember("macs"), data);
            continue;
        }
        if (name == "hostnames") {
            process_hostnames_key(si.getMember("hostnames"), data);
            continue;
        }
        if (name == "fqdns") {
            process_fqdns_key(si.getMember("fqdns"), data);
            continue;
        }
        cxxtools::String value;
        it->getValue(value);
        data[0].push_back(name);
        data[1].push_back(cxxtools::Utf8Codec::encode(value));
    }
}

CsvMap CsvMap_from_serialization_info(const cxxtools::SerializationInfo& si)
{
    std::vector<std::vector<std::string>> data = {{}, {}};
    s_read_si(si, data);
    CsvMap cm{data};
    cm.deserialize();
    return cm;
}


} // namespace fty::asset
