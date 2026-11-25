#include "kx_recipes.h"

namespace kx
{

const char* heat_name(heat_kind value)
{
    switch(value)
    {

    case heat_kind::CAMPFIRE:
        return "CAMPFIRE";

    case heat_kind::STOVE:
        return "STOVE";

    case heat_kind::GAS_STOVE:
        return "GAS STOVE";

    default:
        return "NONE";
    }
}

}
