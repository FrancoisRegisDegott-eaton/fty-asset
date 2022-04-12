#pragma once
#include <cxxtools/serializationinfo.h>
#include <list>
#include <map>
#include <string>

namespace fty {
    class ExtMapElement;
}

// cam/secw interface constants
static constexpr const char *MALAMUTE_ENDPOINT = "ipc://@/malamute";
static constexpr const char *CAM_CLIENT_ID = "asset-agent";
static constexpr const char *CAM_SERVICE_ID = "monitoring";
static constexpr const char *CAM_DEFAULT_PROTOCOL = "monitoring";
static constexpr const char *CAM_DEFAULT_PORT = "0";
static constexpr const int   CAM_TIMEOUT_MS = 1000; // ms

static constexpr const char* SECW_CRED_ID_KEY = "secw_credential_id";

struct CredentialMapping {
    std::string serviceId;
    std::string credentialId;
    std::string protocol;
    std::string port;
};

using ExtMap = std::map<std::string, fty::ExtMapElement>;

std::list<CredentialMapping> getCredentialMappings(const ExtMap& extMap);
void createMappings(const std::string& assetInternalName, const std::list<CredentialMapping>& credentialList);
void deleteMappings(const std::string& assetInternalName);
void deleteAllMappings(const std::string& assetInternalName);

