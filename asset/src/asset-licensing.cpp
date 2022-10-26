#include "asset/asset-licensing.h"
#include <zmq.h>
#include <fty_common_mlm_pool.h>
#include <fty_proto.h>
#include <fty_log.h>
#include <fty/translate.h>
#include <fty/expected.h>

namespace fty::asset {

AssetExpected<LimitationsStruct> getLicensingLimitation()
{
    LimitationsStruct limitations;

    // default values
    limitations.max_active_power_devices = -1;
    limitations.global_configurability = 0;

    // query values

    MlmClientPool::Ptr client_ptr = mlm_pool.get ();
    if (!client_ptr) {
        log_error("mlm_pool get failed");
        return unexpected("mlm_pool get failed."_tr);
    }

    zuuid_t *zuuid = zuuid_new ();
    const char *zuuid_str = zuuid_str_canonical (zuuid);

    zmsg_t *request = zmsg_new();
    zmsg_addstr (request, "LIMITATION_QUERY");
    zmsg_addstr (request, zuuid_str);
    zmsg_addstr (request, "*");
    zmsg_addstr (request, "*");

    int rv = client_ptr->sendto ("etn-licensing", "LIMITATION_QUERY", 5, &request);
    zmsg_destroy (&request);
    if (rv == -1) {
        zuuid_destroy (&zuuid);
        log_fatal ("Cannot send message to etn-licensing");
        return unexpected("mlm_client_sendto failed."_tr);
    }

    zmsg_t *response = client_ptr->recv (zuuid_str, 30);
    zuuid_destroy (&zuuid);
    zuuid_str = NULL;
    if (!response) {
        log_fatal ("client->recv (timeout = '30') returned NULL for LIMITATION_QUERY");
        return unexpected("client->recv () returned NULL"_tr);
    }

    char *reply = zmsg_popstr (response);
    char *status = zmsg_popstr (response);
    if (streq (reply, "REPLY") && streq (status, "OK")) {
        zmsg_t *submsg = zmsg_popmsg(response);
        while (submsg) {
            fty_proto_t *submetric = fty_proto_decode(&submsg);
            zmsg_destroy(&submsg);

            if (submetric
                && (fty_proto_id(submetric) == FTY_PROTO_METRIC)
                && streq (fty_proto_name(submetric), "rackcontroller-0")
            ){
                const char* type = fty_proto_type(submetric); // or quantity, or metric name
                if (streq (type, "power_nodes.max_active")) {
                    limitations.max_active_power_devices = atoi(fty_proto_value (submetric));
                    log_debug("limitations.max_active_power_device set to %d", limitations.max_active_power_devices);
                }
                else if (streq (type, "configurability.global")) {
                    limitations.global_configurability = atoi(fty_proto_value (submetric));
                    log_debug("limitations.global_configurability set to %d", limitations.global_configurability);
                }
            }

            fty_proto_destroy(&submetric);
            submsg = zmsg_popmsg(response);
        }
    }

    zstr_free (&reply);
    zstr_free (&status);
    zmsg_destroy (&response);

    return limitations;
}

}
