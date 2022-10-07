#pragma once
#include "error.h"

namespace fty::asset {

struct LimitationsStruct
{
    int max_active_power_devices{0};
    int global_configurability{0};
};

AssetExpected<LimitationsStruct> getLicensingLimitation();

}


