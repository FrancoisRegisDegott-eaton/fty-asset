/*  =========================================================================
    asset-cam - asset-cam

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

#include "asset-cam.h"

#include <fty_asset_dto.h>
#include <fty_log.h>
#include <fty_security_wallet.h>

std::list<CredentialMapping> getCredentialMappings(const ExtMap& extMap)
{
    std::list<CredentialMapping> credentialList;

    log_debug("Looking for mapping entries");

    auto findCredKey = [] (const auto& el) {
        return el.first.find(SECW_CRED_ID_KEY) != std::string::npos;
    };

    // lookup for ext attributes which contains secw_credential_id in the key
    auto found = std::find_if(extMap.begin(), extMap.end(), findCredKey);

    // create mapping
    while(found != extMap.end()) {
        CredentialMapping c;
        c.credentialId = found->second.getValue();

        // extract protocol from element key (endpoint.XX.protocol.secw_credential_id)
        // auto keyTokens = fty::split(found->first, ".");
        c.serviceId = CAM_SERVICE_ID;
        c.protocol = /* keyTokens.size() >= 3 ? keyTokens[2] : */ CAM_DEFAULT_PROTOCOL;
        c.port = CAM_DEFAULT_PORT;
        credentialList.push_back(c);

        log_debug("Found new credential %s : %s", c.credentialId.c_str(), c.protocol.c_str());

        found = std::find_if(++found, extMap.end(), findCredKey);
    }

    log_debug("Mapping entries found: %zu", credentialList.size());

    return credentialList;
}

void createMappings(const std::string& assetInternalName, const std::list<CredentialMapping>& credentialList)
{
    cam::Accessor camAccessor(CAM_CLIENT_ID, CAM_TIMEOUT_MS, MALAMUTE_ENDPOINT);
    for(const auto& c :credentialList) {
        log_debug("Create new mapping to credential with ID %s", c.credentialId.c_str());
        camAccessor.createMapping(assetInternalName, c.serviceId, c.protocol, c.port, c.credentialId, cam::Status::UNKNOWN /*, empty map */);
    }
}

void deleteMappings(const std::string& assetInternalName)
{
    try {
        log_debug("Deleting monitoring mappings for asset %s", assetInternalName.c_str());
        // remove CAM mappings
        cam::Accessor camAccessor(CAM_CLIENT_ID, CAM_TIMEOUT_MS, MALAMUTE_ENDPOINT);
        //delete only my mapping
        auto mappings = camAccessor.getMappings(assetInternalName, CAM_SERVICE_ID);
        for(const auto& m : mappings) {
            log_debug("Deleting mapping %s : %s", m.m_serviceId.c_str(), m.m_protocol.c_str());
            camAccessor.removeMapping(m.m_assetId, m.m_serviceId, m.m_protocol);
        }
    } catch (const std::exception& e) {
        log_error("Asset mappings could not be removed: %s", e.what());
    }
}

void deleteAllMappings(const std::string& assetInternalName)
{
    try {
        log_debug("Deleting all mappings for asset %s", assetInternalName.c_str());
        // remove CAM mappings
        cam::Accessor camAccessor(CAM_CLIENT_ID, CAM_TIMEOUT_MS, MALAMUTE_ENDPOINT);
        //delete only my mapping
        auto mappings = camAccessor.getAssetMappings(assetInternalName);
        for(const auto& m : mappings) {
            log_debug("Deleting mapping %s : %s (%s)", m.m_serviceId.c_str(), m.m_protocol.c_str(), m.m_serviceId.c_str());
            camAccessor.removeMapping(m.m_assetId, m.m_serviceId, m.m_protocol);
        }
    } catch (const std::exception& e) {
        log_error("Asset mappings could not be removed: %s", e.what());
    }
}
