/*  =========================================================================
    fty-asset - generated layer of public API

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

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
    =========================================================================
*/

#ifndef FTY_ASSET_LIBRARY_H_INCLUDED
#define FTY_ASSET_LIBRARY_H_INCLUDED

//  Set up environment for the application

//  External dependencies
#include <czmq.h>
#include <cxxtools/allocator.h>
#include <tntdb.h>
#include <malamute.h>
#include <magic.h>
#include <openssl/sha.h>
#include <uuid/uuid.h>
#include <fty_log.h>
#include <ftyproto.h>
#include <fty_common_db.h>
#include <fty_common_mlm.h>
#include <fty_common_dto.h>
#include <fty_common_messagebus.h>
#include <fty_asset_activator.h>

//  FTY_ASSET version macros for compile-time API detection
#define FTY_ASSET_VERSION_MAJOR 1
#define FTY_ASSET_VERSION_MINOR 0
#define FTY_ASSET_VERSION_PATCH 0

#define FTY_ASSET_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define FTY_ASSET_VERSION \
    FTY_ASSET_MAKE_VERSION(FTY_ASSET_VERSION_MAJOR, FTY_ASSET_VERSION_MINOR, FTY_ASSET_VERSION_PATCH)

#if defined (__WINDOWS__)
#   if defined FTY_ASSET_STATIC
#       define FTY_ASSET_EXPORT
#   elif defined FTY_ASSET_INTERNAL_BUILD
#       if defined DLL_EXPORT
#           define FTY_ASSET_EXPORT __declspec(dllexport)
#       else
#           define FTY_ASSET_EXPORT
#       endif
#   elif defined FTY_ASSET_EXPORTS
#       define FTY_ASSET_EXPORT __declspec(dllexport)
#   else
#       define FTY_ASSET_EXPORT __declspec(dllimport)
#   endif
#   define FTY_ASSET_PRIVATE
#elif defined (__CYGWIN__)
#   define FTY_ASSET_EXPORT
#   define FTY_ASSET_PRIVATE
#else
#   if (defined __GNUC__ && __GNUC__ >= 4) || defined __INTEL_COMPILER
#       define FTY_ASSET_PRIVATE __attribute__ ((visibility ("hidden")))
#       define FTY_ASSET_EXPORT __attribute__ ((visibility ("default")))
#   else
#       define FTY_ASSET_PRIVATE
#       define FTY_ASSET_EXPORT
#   endif
#endif

//  Opaque class structures to allow forward references
//  These classes are stable or legacy and built in all releases
typedef struct _fty_asset_server_t fty_asset_server_t;
#define FTY_ASSET_SERVER_T_DEFINED
typedef struct _fty_asset_autoupdate_t fty_asset_autoupdate_t;
#define FTY_ASSET_AUTOUPDATE_T_DEFINED
typedef struct _fty_asset_inventory_t fty_asset_inventory_t;
#define FTY_ASSET_INVENTORY_T_DEFINED
typedef struct _fty_asset_dto_t fty_asset_dto_t;
#define FTY_ASSET_DTO_T_DEFINED
typedef struct _asset_conversion_json_t asset_conversion_json_t;
#define ASSET_CONVERSION_JSON_T_DEFINED
typedef struct _asset_conversion_proto_t asset_conversion_proto_t;
#define ASSET_CONVERSION_PROTO_T_DEFINED
typedef struct _asset_conversion_full_asset_t asset_conversion_full_asset_t;
#define ASSET_CONVERSION_FULL_ASSET_T_DEFINED


//  Public classes, each with its own header file
#include "fty_asset_server.h"
#include "fty_asset_autoupdate.h"
#include "fty_asset_inventory.h"
#include "fty_asset_dto.h"
#include "asset/conversion/json.h"
#include "asset/conversion/proto.h"
#include "asset/conversion/full-asset.h"

#ifdef FTY_ASSET_BUILD_DRAFT_API

#ifdef __cplusplus
extern "C" {
#endif

//  Self test for private classes
FTY_ASSET_EXPORT void
    fty_asset_private_selftest (bool verbose, const char *subtest);

#ifdef __cplusplus
}
#endif
#endif // FTY_ASSET_BUILD_DRAFT_API

#endif
/*
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
*/
