/*  =========================================================================
    topology_topology_location - class description

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
    topology_topology_location -
@discuss
@end
*/

#include "topology_location.h"

////////////////////////////////////////////////////////////////////////
// copied from fty-rest
//      see src/wab/tntnet.xml
//      src/web/src/topology_location_from2.ecpp
//      src/web/src/topology_location_from.ecpp
//      src/web/src/topology_location_to.ecpp
// C++ transformed
// see also code in src/topology/
// implementation of REST /api/v1/topology/location (see RFC11)
////////////////////////////////////////////////////////////////////////

#include <stack>
#include <string>
#include <exception>
#include <czmq.h>
#include <fty_common_db_dbpath.h>
#include <fty_common.h>
#include <fty_common_macros.h>
#include <fty_common_utf8.h>

#include <fty_proto.h>

#include "tntdb.h"
#include "topology2.h"
#include "dbtypes.h"
#include "asset/dbhelpers.h"
#include "data.h"
#include "cleanup.h"
#include "assettopology.h"
#include "location_helpers.h"
#include "utilspp.h"


#define REQUEST_DECLINED -1000

/*!
 * \file topology_location_from2.ecpp
 * \author Michal Vyskocil <MichalVyskocil@Eaton.com>
 * \author Barbora Stepankova <BarboraStepankova@Eaton.com>
 * \brief  process location topology requests (from)
 *
 * It is the first file in the chain. Here would be done the complete
 * parameter check. If parameters are ok, but it is not "from" but "to"
 * request control would be delegated to the topology_location_to.ecpp.
 */

//
//

static
int s_topology_location_from2 (std::map<std::string, std::string> & param, std::string & json)
{
    json = "";

    tntdb::Connection conn = tntdb::connect (DBConn::url);

    // ##################################################
    // BLOCK 1
    // Sanity parameter check

    bool checked_recursive = false;
    std::string checked_filter;
    std::string checked_feed_by;
    std::string checked_from;

    {
        std::string from = param["from"];
        std::string to = param["to"];
        std::string filter = param["filter"];
        std::string feed_by = param["feed_by"];
        std::string recursive = param["recursive"];

        // forward to other ecpp files - topology2 does not (yet) support
        // ... unlockated elements or to=
        // ... so fall back to older implementation
        if (from.empty () || !to.empty () || from == "none") {
            log_warning("DECLINED");
            return REQUEST_DECLINED;
        }

        std::transform (recursive.begin(), recursive.end(), recursive.begin(), ::tolower);
        if (recursive == "true") {
            checked_recursive = true;
        }
        else if (!recursive.empty() && recursive != "false") {
            //http_die("request-param-bad", "recursive", recursive.c_str(), "'true'/'false'");
            log_error("request-param-bad, recursive must be 'true'/'false' (%s)", recursive.c_str());
            param["error"] = TRANSLATE_ME("Recursive argument must be 'true'/'false'");
            return -2;
        }

        std::transform (filter.begin(), filter.end(), filter.begin(), ::tolower);
        if (!filter.empty ()) {
            if (filter == "rooms"   ||
                filter == "rows"    ||
                filter == "racks"   ||
                filter == "devices" ||
                filter == "groups" ) {
                checked_filter = filter;
            }
            else {
                //http_die("request-param-bad","filter", filter.c_str(), "'rooms'/'rows'/'racks'/'groups'/'devices'");
                log_error("request-param-bad, filter must be 'rooms'/'rows'/'racks'/'devices'/'groups' (%s)", filter.c_str());
                param["error"] = TRANSLATE_ME("'filter' argument must be 'rooms'/'rows'/'racks'/'devices'/'groups'");
                return -3;
            }
        }

        if (!feed_by.empty ())
        {
            if (filter != "devices") {
                //std::string err =  TRANSLATE_ME("Variable 'feed_by' can be specified only with 'filter=devices'");
                //http_die("parameter-conflict", err.c_str ());
                log_error("parameter-conflict, 'feed_by' can be specified only with 'filter=devices'");
                param["error"] = TRANSLATE_ME("'feed_by' argument can be specified only with 'filter=devices'");
                return -4;
            }
            if (from == "none") {
                //std::string err =  TRANSLATE_ME("With variable 'feed_by' variable 'from' can not be 'none'");
                //http_die("parameter-conflict", err.c_str ());
                log_error("parameter-conflict, with 'feed_by', variable 'from' can not be 'none'");
                return -5;
            }
            if (!persist::is_power_device (conn, feed_by)) {
                //std::string expected = TRANSLATE_ME("must be a power device.");
                //http_die("request-param-bad", "feed_by", feed_by.c_str (), expected.c_str ());
                log_error("request-param-bad, 'feed_by' must be a power device");
                param["error"] = TRANSLATE_ME("'feed_by' argument must be a power device");
                return -6;
            }

            checked_feed_by = feed_by;
        }

        if (from.empty ()) {
            //http_die("request-param-bad", "from");
            log_error("request-param-bad, 'from' is not defined");
            param["error"] = TRANSLATE_ME("'from' argument is not defined");
            return -7;
        }

        checked_from = from;
    }

    log_debug("checked_from '%s', checked_feed_by '%s'", checked_from.c_str(), checked_feed_by.c_str());

    std::set <std::string> fed_by;
    if (!checked_feed_by.empty ())
    {
        fed_by = persist::topology2_feed_by (conn, checked_feed_by);
        if (fed_by.empty ()) {
            //std::string expected = TRANSLATE_ME("must be a device.");
            //http_die("request-param-bad", "feed_by", checked_feed_by.c_str(), expected.c_str ());
            log_error("request-param-bad, 'feed_by' must be a device");
            param["error"] = TRANSLATE_ME("'feed_by' must be a device");
            return -8;
        }
    }

    auto result = persist::topology2_from (conn, checked_from);

    if (result.empty () && checked_from != "none") {
        //std::string expected = TRANSLATE_ME("valid asset name");
        //http_die("request-param-bad", "from", checked_from.c_str(), expected.c_str ());
        log_error("request-param-bad, 'from' is not a valid asset name");
        param["error"] = TRANSLATE_ME("'from' is not a valid asset name (%s)", checked_from.c_str());
        return -9;
    }

    auto groups = persist::topology2_groups (conn, checked_from, checked_recursive);

    std::ostringstream out;

    if (checked_recursive) {
        persist::topology2_from_json_recursive (
            out,
            conn,
            result, checked_from, checked_filter, fed_by, groups
        );
    }
    else {
        persist::topology2_from_json (
            out,
            result, checked_from, checked_filter, fed_by, groups
        );
    }

    json = out.str();

    return 0; // ok
}

/*!
 * \file topology_location_from.ecpp
 * \author Karol Hrdina <KarolHrdina@Eaton.com>
 * \author Michal Hrusecky <MichalHrusecky@Eaton.com>
 * \author Alena Chernikava <AlenaChernikava@Eaton.com>
 * \brief  process location topology requests (from)
 *
 * It is the first file in the chain. Here would be done the complete
 * parameter check. If parameters are ok, but it is not "from" but "to"
 * request control would be delegated to the topology_location_to.ecpp.
 */

//
//

static
int s_topology_location_from (std::map<std::string, std::string> & param, std::string & json)
{
    json = "";

    // ##################################################
    // BLOCK 1
    // Sanity parameter check

    // checked parameters
    a_elmnt_id_t checked_from = 0;
    int checked_recursive = 0;
    int checked_filter = 0;
    a_elmnt_id_t checked_feed_by = 0;

    {
        std::string from = param["from"];
        std::string to = param["to"];
        std::string filter = param["filter"];
        std::string feed_by = param["feed_by"];
        std::string recursive = param["recursive"];

        // 1. Exactly one parameter has to be specified: 'from' or 'to'
        if (!from.empty() && !to.empty()) {
            //std::string err =  TRANSLATE_ME("Only one parameter can be specified at once: 'from' or 'to'");
            //http_die("parameter-conflict", err.c_str ());
            log_error("parameter-conflict, Only one parameter can be specified at once: 'from' or 'to'");
            param["error"] = TRANSLATE_ME("Only one argument can be specified at once: 'from' or 'to'");
            return -1;
        }

        // 2. At least one parameter should be specified
        if (from.empty() && to.empty()) {
            //http_die("request-param-required", "from/to");
            log_error("request-param-required 'from'/'to' is empty");
            param["error"] = TRANSLATE_ME("'from' and 'to' arguments are empty");
            return -2;
        }

        // 3. Check values of 'recursive'
        std::transform (recursive.begin(), recursive.end(), recursive.begin(), ::tolower);
        if (recursive == "true") {
            checked_recursive = 1;
        }
        else if (!recursive.empty() && recursive != "false") {
            //http_die("request-param-bad", "recursive", recursive.c_str(), "'true'/'false'");
            log_error("request-param-bad 'recursive' must be 'true'/'false'");
            param["error"] = TRANSLATE_ME("'recursive' argument must be 'true'/'false'");
            return -3;
        }

        // 4. Check values of 'filter'
        std::transform (filter.begin(), filter.end(), filter.begin(), ::tolower);
        if (filter.empty()) {
            checked_filter = 7;
        }
        else if (filter == "rooms") {
            checked_filter = persist::asset_type::ROOM;
        }
        else if (filter == "rows") {
            checked_filter = persist::asset_type::ROW;
        }
        else if (filter == "racks") {
            checked_filter = persist::asset_type::RACK;
        }
        else if (filter == "devices") {
            checked_filter = persist::asset_type::DEVICE;
        }
        else if (filter == "groups") {
            checked_filter = persist::asset_type::GROUP;
        }
        else {
            // Note: datacenter is not a valid filter parameter according to rfc-11 4.1.13
            //http_die("request-param-bad", "filter", filter.c_str(), "'rooms'/'rows'/'racks'/'groups'/'devices'");
            log_error("request-param-bad, 'filter' must be 'rooms'/'rows'/'racks'/'groups'/'devices'", filter.c_str());
            param["error"] = TRANSLATE_ME("'filter' argument must be 'rooms'/'rows'/'racks'/'groups'/'devices'");
            return -4;
        }

        // 5. Check if 'feed_by' is allowed
        if (!feed_by.empty()) {
            if ( checked_filter != persist::asset_type::DEVICE ) {
                //std::string err =  TRANSLATE_ME("With variable 'feed_by' can be specified only 'filter=devices'");
                //http_die("parameter-conflict", err.c_str ());
                log_error("parameter-conflict, 'feed_by' parameter can be specified only if 'filter=devices'");
                param["error"] = TRANSLATE_ME("'feed_by' argument can be specified only if 'filter=devices'");
                return -5;
            }
            if (from == "none") {
                //std::string err =  TRANSLATE_ME("With variable 'feed_by' variable 'from' can not be 'none'");
                //http_die("parameter-conflict", err.c_str ());
                log_error("parameter-conflict, With parameter 'feed_by', parameter 'from' can not be 'none'");
                param["error"] = TRANSLATE_ME("'from' argument can't be 'none' if 'feed_by' argument is set");
                return -6;
            }
        }

        // 6. if it is not 'from' request, process it as 'to' request in another ecpp file
        if (from.empty()) {
            // pass control to next file in the chain
            log_error("DECLINED");
            return REQUEST_DECLINED; // normally never happens
        }

        // From now on, we are sure, that we are qoing to respond on "location_from" request

        // 6. Check if 'from' has valid asset id
        if (from != "none") {
            //http_errors_t errors;
            //if (!check_element_identifier ("from", from, checked_from, errors)) {
            //    //http_die_error (errors);
            //    log_error ("'from' asset id is not valid (%s)", from.c_str());
            //    return -8;
            //}

            int64_t checked_id = DBAssets::name_to_asset_id (from);
            if (checked_id == -1) {
                //std::string expected = TRANSLATE_ME("existing asset name");
                //http_die ("request-param-bad", "id", asset_id.c_str (), expected.c_str ());
                log_error("request-param-bad 'from' don't exist (%s)", from.c_str());
                param["error"] = TRANSLATE_ME("Asset not found (%s)", from.c_str ());
                return -8;
            }
            if (checked_id == -2) {
                //std::string err =  TRANSLATE_ME("Connecting to database failed.");
                //http_die ("internal-error", err.c_str ());
                log_error("db-connection-failed");
                param["error"] = TRANSLATE_ME("Connection to database failed");
                return -8;
            }
        }

        if ( !feed_by.empty() ) {
            //if ( !check_element_identifier ("feed_by", feed_by, checked_feed_by, errors) ) {
            //    //http_die_error (errors);
            //    log_error ("'feed_by' is not valid (%s)", feed_by.c_str());
            //    return -9;
            //}

            int64_t checked_id = DBAssets::name_to_asset_id (feed_by);
            if (checked_id == -1) {
                //std::string expected = TRANSLATE_ME("existing asset name");
                //http_die ("request-param-bad", "id", asset_id.c_str (), expected.c_str ());
                log_error("request-param-bad 'feed_by' don't exist (%s)", feed_by.c_str());
                param["error"] = TRANSLATE_ME("Asset not found (%s)", feed_by.c_str ());
                return -9;
            }
            if (checked_id == -2) {
                //std::string err =  TRANSLATE_ME("Connecting to database failed.");
                //http_die ("internal-error", err.c_str ());
                log_error("db-connection-failed");
                param["error"] = TRANSLATE_ME("Connection to database failed");
                return -9;
            }

            asset_manager asset_mgr;
            auto tmp = asset_mgr.get_item1 (checked_feed_by);
            if (tmp.status == 0) {
                switch (tmp.errsubtype) {
                    case DB_ERROR_NOTFOUND:
                        //http_die("element-not-found", std::to_string(checked_feed_by).c_str());
                        log_error("element-not-found 'feed_by' (%s)", feed_by.c_str());
                        param["error"] = TRANSLATE_ME("Asset not found (%s)", feed_by.c_str ());
                        return -10;
                    case DB_ERROR_BADINPUT:
                    case DB_ERROR_INTERNAL:
                    default:
                        //http_die("internal-error", tmp.msg.c_str());
                        log_error("internal-error %s", tmp.msg.c_str());
                        param["error"] = TRANSLATE_ME("Internal error");
                }
                return -11;
            }
            if ( tmp.item.basic.type_id != persist::asset_type::DEVICE ) {
                //std::string expected = TRANSLATE_ME("be a device");
                //http_die("request-param-bad", "feed_by", feed_by.c_str(), expected.c_str ());
                log_error("request-param-bad, 'feed_by' must be a device (%s)", feed_by.c_str());
                param["error"] = TRANSLATE_ME("'feed_by' argument must decribe a device (%s)", feed_by.c_str ());
                return -12;
            }
        }
    }
    // Sanity check end

    // ##################################################
    // BLOCK 2
    // Call persistence layer
    _scoped_asset_msg_t *input_msg = asset_msg_new (ASSET_MSG_GET_LOCATION_FROM);
    assert (input_msg);
    asset_msg_set_element_id (input_msg, checked_from);
    asset_msg_set_recursive (input_msg, static_cast<byte>(checked_recursive));
    asset_msg_set_filter_type (input_msg, static_cast<byte>(checked_filter));

    _scoped_zmsg_t *return_msg = process_assettopology (DBConn::url.c_str(), &input_msg, checked_feed_by);
    if (return_msg == NULL) {
        log_error ("Function process_assettopology() returned a null pointer");
        //http_die("internal-error", "");
        param["error"] = TRANSLATE_ME("Internal error");
        return -20;
    }

    if (is_common_msg (return_msg)) {
        _scoped_common_msg_t *common_msg = common_msg_decode (&return_msg);
        zmsg_destroy (&return_msg);
        if (common_msg == NULL) {
            log_error ("common_msg_decode() failed");
            //http_die("internal-error", "");
            param["error"] = TRANSLATE_ME("Internal error");
            return -21;
        }

        if (common_msg_id (common_msg) == COMMON_MSG_FAIL) {
            log_error ("common_msg is COMMON_MSG_FAIL");
            switch(common_msg_errorno(common_msg)) {
                case(DB_ERROR_NOTFOUND):
                    //http_die("element-not-found", std::to_string(checked_from).c_str());
                    log_error("element-not-found %s", std::to_string(checked_from).c_str());
                    param["error"] = TRANSLATE_ME("Asset not found (%s)", param["from"].c_str());
                    return -22;
                case(DB_ERROR_BADINPUT): // this should never be returned
                default:
                    //http_die("internal-error", "");
                    log_error("internal-error");
                    param["error"] = TRANSLATE_ME("Internal error");
            }
            return -23;
        }
        else {
            log_error ("Unexpected common_msg received. ID = %" PRIu32 , common_msg_id (common_msg));
            //http_die("internal-error", "");
            param["error"] = TRANSLATE_ME("Internal error");
            return -24;
        }
    }
    else if (is_asset_msg (return_msg)) {
        _scoped_asset_msg_t *asset_msg = asset_msg_decode (&return_msg);
        zmsg_destroy (&return_msg);
        if (asset_msg == NULL) {
            log_error ("asset_msg_decode() failed");
            //http_die("internal-error", "");
            param["error"] = TRANSLATE_ME("Internal error");
            return -25;
        }
        if (asset_msg_id (asset_msg) != ASSET_MSG_RETURN_LOCATION_FROM) {
            log_error ("Unexpected asset_msg received. ID = %" PRIu32 , asset_msg_id (asset_msg));
            //http_die("internal-error", "");
            param["error"] = TRANSLATE_ME("Internal error");
            return -26;
        }
        if (asset_location_r(&asset_msg, json) != 0) {
            log_error ("unexpected error, during the reply parsing");
            //http_die("internal-error", "");
            param["error"] = TRANSLATE_ME("Internal error");
            return -27;
        }
    }
    else {
        zmsg_destroy (&return_msg);
        log_error ("Unknown protocol");
        //http_die("internal-error", "");
        param["error"] = TRANSLATE_ME("Internal error");
        return -28;
    }
    zmsg_destroy (&return_msg);

    return 0; // ok
}

/*!
 * \file topology_location_to.ecpp
 * \author Karol Hrdina <KarolHrdina@Eaton.com>
 * \author Michal Hrusecky <MichalHrusecky@Eaton.com>
 * \author Alena Chernikava <AlenaChernikava@Eaton.com>
 * \brief process location topology requests (to)
 */

//
//

static
int s_topology_location_to (std::map<std::string, std::string> & param, std::string & json)
{
    json = "";

    // ##################################################
    // BLOCK 1
    // Sanity parameter check

    // checked parameters
    std::string checked_to;

    {
        std::string to = param["to"];
        std::string filter = param["filter"];
        std::string feed_by = param["feed_by"];
        std::string recursive = param["recursive"];

        if (to.empty()) {
            // At least one parametr should be specified
            //http_die("request-param-required", "from/to");
            log_error("request-param-required, 'to' not defined");
            param["error"] = TRANSLATE_ME("'to' argument is not defined");
            return -1;
        }

        // 1. 'recursive' cannot be specified with 'to'
        if (!recursive.empty()) {
            //std::string err =  TRANSLATE_ME("Parameter 'recursive' can not be specified with parameter 'to'.");
            //http_die("parameter-conflict", err.c_str ());
            log_error("parameter-conflict, 'recursive' can not be specified with parameter 'to'");
            param["error"] = TRANSLATE_ME("'recursive' argument can not be specified with argument 'to'");
            return -2;
        }

        // 2. 'filter' cannot be specified with 'to'
        if (!filter.empty()) {
            //std::string err =  TRANSLATE_ME("Parameter 'filter' can not be specified with parameter 'to'.");
            //http_die("parameter-conflict", err.c_str ());
            log_error("parameter-conflict, 'filter' can not be specified with parameter 'to'");
            param["error"] = TRANSLATE_ME("'filter' argument can not be specified with argument 'to'");
            return -3;
        }

        // 3. 'feed_by' cannot be specified with 'to'
        if (!feed_by.empty()) {
            //std::string err =  TRANSLATE_ME("Parameter 'feed_by' can not be specified with parameter 'to'.");
            //http_die("parameter-conflict", err.c_str ());
            log_error("parameter-conflict, 'feed_by' can not be specified with parameter 'to'");
            param["error"] = TRANSLATE_ME("'feed_by' argument can not be specified with argument 'to'");
            return -4;
        }

        if (!persist::is_ok_name (to.c_str ())) {
            //http_die ("request-param-required", to.c_str() ,"to");
            log_error ("request-param-required, 'to' is not defined (%s)", to.c_str());
            param["error"] = TRANSLATE_ME("Asset not defined (%s)", to.c_str());
            return -5;
        }

        checked_to = to;
    }
    // Sanity check end

    int64_t checked_to_num = DBAssets::name_to_asset_id (checked_to);
    if (checked_to_num == -1) {
        //std::string expected = TRANSLATE_ME("existing asset name");
        //http_die ("request-param-bad", "to", checked_to.c_str (), expected.c_str ());
        log_error ("request-param-bad, 'to' is not a valid asset id (%s)", checked_to.c_str());
        param["error"] = TRANSLATE_ME("Asset not found (%s)", param["to"].c_str());
        return -6;
    }
    if (checked_to_num == -2) {
        //std::string err =  TRANSLATE_ME("Connecting to database failed.");
        //http_die ("internal-error", err.c_str ());
        log_error ("internal-error, connection to database failed");
        param["error"] = TRANSLATE_ME("Connection to database failed");
        return -7;
    }

    // ##################################################
    // BLOCK 2
    // Call persistence layer
    _scoped_asset_msg_t *input_msg = asset_msg_new (ASSET_MSG_GET_LOCATION_TO);
    assert (input_msg);
    asset_msg_set_element_id (input_msg, static_cast<uint32_t>(checked_to_num));
    _scoped_zmsg_t *return_msg = process_assettopology (DBConn::url.c_str(), &input_msg);
    asset_msg_destroy(&input_msg);

    if (return_msg == NULL) {
        log_error ("Function process_assettopology() returned a null pointer");
        //http_die("internal-error", "");
        log_error("internal-error");
        param["error"] = TRANSLATE_ME("Internal error");
        return -8;
    }

    if (is_common_msg (return_msg)) {
        _scoped_common_msg_t *common_msg = common_msg_decode (&return_msg);
        zmsg_destroy (&return_msg);

        if (common_msg == NULL) {
            log_error ("common_msg_decode() failed");
            //http_die("internal-error", "");
            log_error("internal-error");
            param["error"] = TRANSLATE_ME("Internal error");
            return -9;
        }

        if (common_msg_id (common_msg) == COMMON_MSG_FAIL) {
            log_error ("common_msg is COMMON_MSG_FAIL");
            switch(common_msg_errorno(common_msg)) {
                case(DB_ERROR_NOTFOUND):
                    //http_die("element-not-found", checked_to.c_str());
                    log_error("element-not-found (%s)", checked_to.c_str());
                    param["error"] = TRANSLATE_ME("Asset not found (%s)", checked_to.c_str());
                    common_msg_destroy(&common_msg);
                    return -10;
                case(DB_ERROR_BADINPUT):
                default:;
            }
            //http_die("internal-error", "");
            log_error("internal-error");
            common_msg_destroy(&common_msg);
            param["error"] = TRANSLATE_ME("Internal error");
            return -11;
        }
        else {
            log_error ("Unexpected common_msg received. ID = %" PRIu32 , common_msg_id (common_msg));
            //http_die("internal-error", "");
            common_msg_destroy(&common_msg);
            param["error"] = TRANSLATE_ME("Internal error");
            return -12;
        }
    }
    else if (is_asset_msg (return_msg)) {
        _scoped_asset_msg_t *asset_msg = asset_msg_decode (&return_msg);
        zmsg_destroy (&return_msg);

        if (asset_msg == NULL) {
            log_error ("asset_msg_decode() failed");
            //http_die("internal-error", "");
            param["error"] = TRANSLATE_ME("Internal error");
            return -13;
        }

        if (asset_msg_id (asset_msg) == ASSET_MSG_RETURN_LOCATION_TO)
        {
            // <checked_to_num, type, contains, name, type_name>
            std::stack<std::tuple <int, int, std::string, std::string, std::string>> stack;
            std::string contains; // empty

            while (1) {
                int64_t checked_to_num1 = static_cast<int64_t>(asset_msg_element_id (asset_msg));
                int type_id = asset_msg_type (asset_msg);
                std::string name = asset_msg_name (asset_msg);
                std::string type_name = asset_msg_type_name (asset_msg);

                stack.push (make_tuple(checked_to_num1, type_id, contains, name, type_name));

                // I deliberately didn't want to use asset manager (unknown / ""; suffix s)
                // TODO use special function
                switch (type_id) {
                    case persist::asset_type::DATACENTER:
                        contains = "datacenters";
                        break;
                    case persist::asset_type::ROOM:
                        contains = "rooms";
                        break;
                    case persist::asset_type::ROW:
                        contains = "rows";
                        break;
                    case persist::asset_type::RACK:
                        contains = "racks";
                        break;
                    case persist::asset_type::GROUP:
                        contains = "groups";
                        break;
                    case persist::asset_type::DEVICE:
                        contains = "devices";
                        break;
                    default: {
                        log_error ("Unexpected asset type received in the response");
                        //http_die("internal-error", "");
                        asset_msg_destroy (&asset_msg);
                        param["error"] = TRANSLATE_ME("Internal error");
                        return -14;
                    }
                }

                if (zmsg_size (asset_msg_msg (asset_msg)) == 0) {
                    break; // while
                }

                // next
                _scoped_zmsg_t *inner = asset_msg_get_msg (asset_msg);
                asset_msg_destroy (&asset_msg);
                asset_msg = asset_msg_decode (&inner);
                zmsg_destroy (&inner);

                if (asset_msg == NULL) {
                    log_error ("asset_msg_decode() failed");
                    //http_die("internal-error", "");
                    param["error"] = TRANSLATE_ME("Internal error");
                    return -15;
                }
            }
            asset_msg_destroy (&asset_msg); // useless

            // Now go from top -> down, build json payload

            int counter = 0; // for 'contains'
            int indent = 0;
            std::string ext_name;

            json = "{\n";

            while (!stack.empty()) {
                // <checked_to_num, type, contains, name, type_name>
                std::tuple<int, int, std::string, std::string, std::string> row = stack.top();
                stack.pop();

                std::pair <std::string,std::string> asset_names = DBAssets::id_to_name_ext_name (static_cast<uint32_t>(std::get<0>(row)));
                if (asset_names.first.empty () && asset_names.second.empty ()) {
                    //std::string err =  TRANSLATE_ME("Database failure");
                    //http_die ("internal-error", err.c_str ());
                    log_error("Database failure");
                    param["error"] = TRANSLATE_ME("Database access failed");
                    return -16;
                }
                ext_name = asset_names.second;

                indent++;
                if (!std::get<2>(row).empty()) {
                    for (int i = 0; i < indent; i++) {
                        json.append ("\t");
                    }
                    json.append("\"name\" : \"")
                        .append(ext_name)
                        .append("\",\n");
                    for (int i = 0; i < indent; i++) {
                        json.append ("\t");
                    }
                    json.append("\"id\" : \"")
                        .append(std::get<3>(row))
                        .append("\",\n");
                    if (std::get<4>(row) != "N_A") { // magic constant from initdb.sql
                        for (int i = 0; i < indent; i++) {
                            json.append ("\t");
                        }
                        json.append("\"type\" : \"")
                            .append(std::get<4>(row))
                            .append("\"\n");
                    }
                    for (int i = 0; i < indent; i++) {
                        json.append ("\t");
                    }

                    counter++;
                    json.append("\"contains\" : { \"")
                        .append(std::get<2>(row))
                        .append("\" : [{\n");
                }
                else {
                    for (int i = 0; i < indent; i++) {
                        json.append ("\t");
                    }
                    json.append("\"name\" : \"")
                        .append(ext_name)
                        .append("\",\n");
                    for (int i = 0; i < indent; i++) {
                        json.append ("\t");
                    }
                    json.append("\"id\" : \"")
                        .append(std::get<3>(row))
                        .append("\"");
                    json.append(",\n");
                    json += "\"type\" : \"" + persist::typeid_to_type(static_cast<uint16_t>(std::get<1>(row))) + "\",";
                    for (int i = 0; i < indent; i++) {
                        json.append ("\t");
                    }
                    json.append("\"sub_type\" : \"")
                        .append(utils::strip (std::get<4>(row)))
                        .append("\"\n");
                }
            }

            // close contains objects
            for (int i = counter; i > 0; i--) {
                indent--;
                for (int j = 0; j < indent; j++) {
                    json.append ("\t");
                }
                json.append ("}]}\n");
            }

            json.append ("}");
        }
        else {
            log_error ("Unexpected asset_msg received. ID = %" PRIu32 , asset_msg_id (asset_msg));
            //http_die("internal-error", "");
            asset_msg_destroy (&asset_msg);
            param["error"] = TRANSLATE_ME("Internal error");
            return -17;
        }
        asset_msg_destroy (&asset_msg);
    }
    else {
        zmsg_destroy (&return_msg);
        log_error ("Unknown protocol");
        //http_die("internal-error", "");
        param["error"] = TRANSLATE_ME("Internal error");
        return -18;
    }
    zmsg_destroy (&return_msg);

    return 0; // ok
}

//  topology_location main entry
//  PARAM map keys in from/to with recursive/filter/feed_by specific arguments
//  attempt: from/to: assetID, recursive in 'true'/'false', filter asset, feed_by device
//  source: fty-rest /api/v1/topology/location REST api
//      src/web/tntnet.xml, src/web/src/topology_location_[from|from2].ecpp, src/web/src/topology_location_to.ecpp
//  returns 0 if success (json payload is valid), else <0

int topology_location (std::map<std::string, std::string> & param, std::string & json)
{
    json = "";

    if (!param["from"].empty()) {
        int r = s_topology_location_from2(param, json);
        if (r == REQUEST_DECLINED) {
            log_trace("s_topology_location_from2() has declined, redirect to s_topology_location_from()");
            r = s_topology_location_from(param, json);
            if (r != 0) {
                log_error("topology_location_from failed, r: %d", r);
                return -1;
            }
        }
        else if (r != 0) {
            log_error("topology_location_from2 failed, r: %d", r);
            return -2;
        }

        return 0; // ok
    }

    if (!param["to"].empty()) {
        int r = s_topology_location_to(param, json);
        if (r != 0) {
            log_error("topology_location_to failed, r: %d", r);
            return -3;
        }
        return 0; // ok
    }

    log_error("unexpected parameter, 'from'/'to' must be defined");
    param["error"] = TRANSLATE_ME("'from' or 'to' argument must be defined");
    return -3; // unexpected param
}
