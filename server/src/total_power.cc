/*  =========================================================================
    total_power - Calculation of total power for one asset

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
    total_power - Calculation of total power for one asset
@discuss
@end
*/

#include "total_power.h"
#include "asset/dbhelpers.h"

#include <fty_log.h>
#include <fty_common.h>

#include <tntdb/connect.h>
#include <tntdb/result.h>
#include <tntdb/error.h>
#include <exception>

class ShortAssetInfo {
public:
    uint32_t asset_id;
    std::string asset_name;
    uint16_t subtype_id;

    ShortAssetInfo (uint32_t aasset_id, const std::string &aasset_name, uint16_t asubtype_id)
    {
        asset_id = aasset_id;
        asset_name = aasset_name;
        subtype_id = asubtype_id;
    };
};

inline bool operator<(const ShortAssetInfo& lhs, const ShortAssetInfo& rhs)
{
  return lhs.asset_id < rhs.asset_id;
}

/**
 * \brief Simple wrapper to make code more readable
 */
static bool
    is_ups (
        const ShortAssetInfo &device
    )
{
    if ( device.subtype_id == persist::asset_subtype::UPS ) {
        return true;
    }
    else {
        return false;
    }
}

/**
 * \brief Simple wrapper to make code more readable
 */
static bool
    is_epdu (
        const ShortAssetInfo &device
    )
{
    if ( device.subtype_id == persist::asset_subtype::EPDU ) {
        return true;
    }
    else {
        return false;
    }
}

/**
 *  \brief For the specified asset it derives a set of powered devices
 *
 *  From set of links derives set of elements that at least once were
 *              in a role of a destination device in a link.
 *  Link is: from to
 *
 *  \param[in] links - information about all links, where at least one end
 *                          belongs to the devices in the container
 *  \param[in] element_id - powering device id
 *
 *  \return a set of powered devices. It can be empty.
 */
static std::set<uint32_t>
    find_dests (
        const std::set <std::pair<uint32_t, uint32_t> > &links,
        uint32_t element_id
    )
{
    std::set<uint32_t> dests;

    for ( auto &one_link: links )
    {
        if ( one_link.first == element_id )
            dests.insert(one_link.second);
    }
    return dests;
}
/**
 *  \brief Checks if some power device is directly powering devices
 *          in some other racks.
 *
 *  \param[in] device - device to check
 *  \param[in] devices_in_container - information about all devices in the
 *                          asset container
 *  \param[in] links - information about all links, where at least one end
 *                          belongs to the devices in the container
 *
 *  \return true or false
 */
static bool
    is_powering_other_rack (
        const ShortAssetInfo &device,
        const std::map <uint32_t, ShortAssetInfo> &devices_in_container,
        const std::set <std::pair<uint32_t, uint32_t> > &links
    )
{
    auto adevice_dests = find_dests (links, device.asset_id);
    for ( auto &adevice: adevice_dests )
    {
        auto it = devices_in_container.find(adevice);
        if ( it == devices_in_container.cend() ) {
            // it means, that destination device is out of the container
            return true;
        }
    }
    return false;
}

/**
 *  \brief From old set of border devices generates a new set of border devices
 *
 *  Each device "A" is replaced with the list of devices that were powered by
 *  this device "A".
 *  A border device is a device, that should be taken as a power
 *  source device at first turn.
 *
 *  \param[in] devices_in_container - information about all devices in the
 *                          asset container
 *  \param[in] links - information about all links, where at least one end
 *                          belongs to the devices in the container
 *  \param[in][out] border_devices - list of border devices to be updated
 */
static void
    update_border_devices (
        const std::map <uint32_t, ShortAssetInfo> &container_devices,
        const std::set <std::pair<uint32_t, uint32_t> > &links,
        std::set <ShortAssetInfo> &border_devices
    )
{
    std::set<ShortAssetInfo> new_border_devices;
    for ( auto &border_device: border_devices )
    {
        auto adevice_dests = find_dests (links, border_device.asset_id);
        for ( auto &adevice: adevice_dests )
        {
            auto it = container_devices.find(adevice);
            if ( it != container_devices.cend() )
                new_border_devices.insert(it->second);
            else
            {
                log_error ("DB can be in inconsistant state or some device "
                        "has power source in the other container");
                log_error ("device(as element) %" PRIu32 " is not in container",
                                                adevice);
                // do nothing in this case
            }
        }
    }
    border_devices.clear();
    border_devices.insert(new_border_devices.begin(),
                          new_border_devices.end());
}

/**
 *  \brief An implementation of the algorithm.
 *
 *  GOAL: Take the first "smart" devices in every powerchain that are
 *        the closest to "feed".
 *
 *  If the found device is not smart, try to look at upper level. Repeat until
 *  chain ends or until all chains are processed.
 *
 *  Asset container - is an asset for which we want to compute the totl power
 *  Asset container devices - all devices placed in the asset container.
 *
 *  \param[in] devices_in_container - information about all devices in the
 *                          asset container
 *  \param[in] links - information about all links, where at least one end
 *                          belongs to the devices in the container
 *
 *  \return a list of power devices names. It there are no power devices the
 *          list is empty.
 */
static std::vector<std::string>
    total_power_v2 (
        const std::map <uint32_t, ShortAssetInfo> &devices_in_container,
        const std::set <std::pair<uint32_t, uint32_t> > &links
    )
{

    // the set of all border devices ("starting points")
    std::set <ShortAssetInfo> border_devices;
    // the set of all destination devices in selected links
    std::set <uint32_t> dest_dvcs{};
    //  from (first)   to (second)
    //           +--------------+
    //  B________|______A__C    |
    //           |              |
    //           +--------------+
    //   B is out of the Container
    //   A is in the Container
    //   then A is border device
    for ( auto &oneLink : links ) {
        log_debug ("  cur_link: %d->%d", oneLink.first, oneLink.second);
        auto it = devices_in_container.find (oneLink.first);
        if ( it == devices_in_container.end() )
            // if in the link first point is out of the Container,
            // the second definitely should be in Container,
            // otherwise it is not a "container"-link
        {
            border_devices.insert(
                    devices_in_container.find(oneLink.second)->second);
        }
        dest_dvcs.insert(oneLink.second);
    }
    //  from (first)   to (second)
    //           +-----------+
    //           |A_____C    |
    //           |           |
    //           +-----------+
    //   A is in the Container (from)
    //   C is in the Container (to)
    //   then A is border device
    //
    //   Algorithm: from all devices in the Container we will
    //   select only those that don't have an incoming links
    //   (they are not a destination device for any link)
    for ( auto &oneDevice : devices_in_container ) {
        if ( dest_dvcs.find (oneDevice.first) == dest_dvcs.end() ) {
            border_devices.insert ( oneDevice.second );
        }
    }

    std::vector <std::string> dvc{};
    if ( border_devices.empty() ) {
        return dvc;
    }
    // it is not a good idea to delete from collection while iterating it
    std::set <ShortAssetInfo> todelete{};
    while ( !border_devices.empty() ) {
        for ( auto &border_device: border_devices ) {
            if ( ( is_epdu(border_device) ) ||
                 ( ( is_ups(border_device) ) &&
                   ( !is_powering_other_rack (border_device, devices_in_container, links) ) ) )
            {
                dvc.push_back(border_device.asset_name);
                // remove from border
                todelete.insert(border_device);
                continue;
            }
            // NOT IMPLEMENTED
            //if ( is_it_device(border_device) )
            //{
            //    // remove from border
            //    // add to ipmi
            //}
        }
        for (auto &todel: todelete) {
            border_devices.erase(todel);
        }
        update_border_devices(devices_in_container, links, border_devices);
    }
    return dvc;
}

/**
 *  \brief For the specified asset finds out the devices
 *        that are used for total power computation
 *
 *  \param[in] assetId - id of the asset, for which we need to
 *                       determine devices for total power calculation
 *  \param[out] powerDevices - list of devices for the specified asset
 *                       used for total power computation.
 *                       It's content would be cleared every time
 *                       at the beginning.
 *
 *  \return  0 - in case of success
 *          -1 - in case of internal error
 *          -2 - in case the requested asset was not found
 */
static int
    select_total_power_by_id (
        tntdb::Connection &conn,
        uint32_t assetId,
        std::vector<std::string> &powerDevices
    )
{
    // at the beginning clear
    powerDevices.clear();
    // select all devices in the container
    std::map <uint32_t, ShortAssetInfo> container_devices{};
    std::function<void(const tntdb::Row&)> func = \
                [&container_devices](const tntdb::Row& row)
                {
                    uint16_t type_id = 0;
                    row["type_id"].get(type_id);

                    if ( type_id == persist::asset_type::DEVICE ) {
                        std::string device_name = "";
                        row["name"].get(device_name);

                        uint32_t asset_id = 0;
                        row["asset_id"].get(asset_id);

                        uint16_t subtype_id = 0;
                        row["subtype_id"].get(subtype_id);

                        container_devices.emplace (asset_id,
                                ShortAssetInfo(asset_id, device_name,
                                    subtype_id));
                    }
                };

    auto rv = DBAssets::select_assets_by_container (conn, assetId, func);

    // here would be placed names of devices to summ up
    if ( rv != 0 ) {
        log_warning ("asset_id='%" PRIu32 "': problems appeared in selecting devices",
                assetId);
        // so return an empty set of power devices
        powerDevices = {};
        return -1;
    }
    if ( container_devices.empty() ) {
        log_debug ("asset_id='%" PRIu32 "': has no devices", assetId);
        // so return an empty set of power devices
        powerDevices = {};
        return 0;
    }

    auto ret_active = DBAssets::select_links_by_container (conn, assetId, "active");
    std::set <std::pair<uint32_t ,uint32_t> > links = ret_active.item;
    //rv = select_links_by_container (assetId, links);
    if ( ret_active.status == 0 ) {
        log_warning ("asset_id='%" PRIu32 "': internal problems in links detecting",
                assetId);
        // so return an empty set of power devices
        powerDevices = {};
        return -1;
    }

    auto ret_nonactive = DBAssets::select_links_by_container (conn, assetId, "nonactive");
    links.insert (ret_nonactive.item.begin (), ret_nonactive.item.end ());
    if ( ret_nonactive.status == 0 ) {
        log_warning ("asset_id='%" PRIu32 "': internal problems in links detecting",
                assetId);
        // so return an empty set of power devices
        powerDevices = {};
        return -1;
    }

    if ( links.empty() ) {
        log_debug ("asset_id='%" PRIu32 "': has no power links", assetId);
        // so return an empty set of power devices
        powerDevices = {};
        return 0;
    }

    powerDevices = total_power_v2 (container_devices, links);
    return 0;
}

int
    select_devices_total_power(
        const std::string &assetName,
        std::vector<std::string> &powerDevices,
        bool test
    )
{
    if (test) {
        log_debug("[select_devices_total_power]: runs in test mode");
        return 0;
    }

    tntdb::Connection conn = tntdb::connectCached (DBConn::url);
    int64_t assetId = DBAssets::name_to_asset_id (assetName);
    if ( assetId < 0 ) {
        return static_cast<int>(assetId);
    }
    return select_total_power_by_id (conn, static_cast<uint32_t>(assetId), powerDevices);
}
