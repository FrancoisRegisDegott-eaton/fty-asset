/*  =========================================================================
    fty_asset_classes - private header file

    Copyright (C) 2016 - 2020 Eaton

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

#pragma once

//  Platform definitions, must come first
#include "unicode/platform.h"

//  Asserts check the invariants of methods. If they're not
//  fulfilled the program should fail fast. Therefore enforce them!
#include <cassert>


//  Opaque class structures to allow forward references
#ifndef TOPOLOGY_DB_TOPOLOGY2_T_DEFINED
typedef struct _topology_db_topology2_t topology_db_topology2_t;
#define TOPOLOGY_DB_TOPOLOGY2_T_DEFINED
#endif
#ifndef TOPOLOGY_PERSIST_DBHELPERS2_T_DEFINED
typedef struct _topology_persist_dbhelpers2_t topology_persist_dbhelpers2_t;
#define TOPOLOGY_PERSIST_DBHELPERS2_T_DEFINED
#endif
#ifndef TOPOLOGY_PERSIST_ASSETCRUD_T_DEFINED
typedef struct _topology_persist_assetcrud_t topology_persist_assetcrud_t;
#define TOPOLOGY_PERSIST_ASSETCRUD_T_DEFINED
#endif
#ifndef TOPOLOGY_PERSIST_ASSETTOPOLOGY_T_DEFINED
typedef struct _topology_persist_assettopology_t topology_persist_assettopology_t;
#define TOPOLOGY_PERSIST_ASSETTOPOLOGY_T_DEFINED
#endif
#ifndef TOPOLOGY_PERSIST_MONITOR_T_DEFINED
typedef struct _topology_persist_monitor_t topology_persist_monitor_t;
#define TOPOLOGY_PERSIST_MONITOR_T_DEFINED
#endif
#ifndef TOPOLOGY_PERSIST_PERSIST_ERROR_T_DEFINED
typedef struct _topology_persist_persist_error_t topology_persist_persist_error_t;
#define TOPOLOGY_PERSIST_PERSIST_ERROR_T_DEFINED
#endif
#ifndef TOPOLOGY_SHARED_ASSET_GENERAL_T_DEFINED
typedef struct _topology_shared_asset_general_t topology_shared_asset_general_t;
#define TOPOLOGY_SHARED_ASSET_GENERAL_T_DEFINED
#endif
#ifndef TOPOLOGY_SHARED_DATA_T_DEFINED
typedef struct _topology_shared_data_t topology_shared_data_t;
#define TOPOLOGY_SHARED_DATA_T_DEFINED
#endif
#ifndef TOPOLOGY_SHARED_UTILSPP_T_DEFINED
typedef struct _topology_shared_utilspp_t topology_shared_utilspp_t;
#define TOPOLOGY_SHARED_UTILSPP_T_DEFINED
#endif
#ifndef TOPOLOGY_SHARED_LOCATION_HELPERS_T_DEFINED
typedef struct _topology_shared_location_helpers_t topology_shared_location_helpers_t;
#define TOPOLOGY_SHARED_LOCATION_HELPERS_T_DEFINED
#endif
#ifndef TOPOLOGY_MSG_ASSET_MSG_T_DEFINED
typedef struct _topology_msg_asset_msg_t topology_msg_asset_msg_t;
#define TOPOLOGY_MSG_ASSET_MSG_T_DEFINED
#endif
#ifndef TOPOLOGY_MSG_COMMON_MSG_T_DEFINED
typedef struct _topology_msg_common_msg_t topology_msg_common_msg_t;
#define TOPOLOGY_MSG_COMMON_MSG_T_DEFINED
#endif
#ifndef TOPOLOGY_TOPOLOGY_POWER_T_DEFINED
typedef struct _topology_topology_power_t topology_topology_power_t;
#define TOPOLOGY_TOPOLOGY_POWER_T_DEFINED
#endif
#ifndef TOPOLOGY_TOPOLOGY_LOCATION_T_DEFINED
typedef struct _topology_topology_location_t topology_topology_location_t;
#define TOPOLOGY_TOPOLOGY_LOCATION_T_DEFINED
#endif
#ifndef TOPOLOGY_TOPOLOGY_INPUT_POWERCHAIN_T_DEFINED
typedef struct _topology_topology_input_powerchain_t topology_topology_input_powerchain_t;
#define TOPOLOGY_TOPOLOGY_INPUT_POWERCHAIN_T_DEFINED
#endif
#ifndef TOPOLOGY_PROCESSOR_T_DEFINED
typedef struct _topology_processor_t topology_processor_t;
#define TOPOLOGY_PROCESSOR_T_DEFINED
#endif
#ifndef DBHELPERS_T_DEFINED
typedef struct _dbhelpers_t dbhelpers_t;
#define DBHELPERS_T_DEFINED
#endif
#ifndef TOTAL_POWER_T_DEFINED
typedef struct _total_power_t total_power_t;
#define TOTAL_POWER_T_DEFINED
#endif
#ifndef DNS_T_DEFINED
typedef struct _dns_t dns_t;
#define DNS_T_DEFINED
#endif
#ifndef ASSET_SERVER_T_DEFINED
typedef struct _asset_server_t asset_server_t;
#define ASSET_SERVER_T_DEFINED
#endif
#ifndef ASSET_ASSET_T_DEFINED
typedef struct _asset_asset_t asset_asset_t;
#define ASSET_ASSET_T_DEFINED
#endif
#ifndef ASSET_ASSET_UTILS_T_DEFINED
typedef struct _asset_asset_utils_t asset_asset_utils_t;
#define ASSET_ASSET_UTILS_T_DEFINED
#endif
#ifndef ASSET_ASSET_STORAGE_T_DEFINED
typedef struct _asset_asset_storage_t asset_asset_storage_t;
#define ASSET_ASSET_STORAGE_T_DEFINED
#endif
#ifndef ASSET_ASSET_DB_T_DEFINED
typedef struct _asset_asset_db_t asset_asset_db_t;
#define ASSET_ASSET_DB_T_DEFINED
#endif
#ifndef ASSET_ASSET_DB_TEST_T_DEFINED
typedef struct _asset_asset_db_test_t asset_asset_db_test_t;
#define ASSET_ASSET_DB_TEST_T_DEFINED
#endif

//  *** To avoid double-definitions, only define if building without draft ***
#ifndef FTY_ASSET_BUILD_DRAFT_API

#endif // FTY_ASSET_BUILD_DRAFT_API
