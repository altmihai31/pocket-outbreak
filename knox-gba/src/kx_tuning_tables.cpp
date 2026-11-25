#include "kx_camera_tuning.h"
#include "kx_combat_tuning.h"
#include "kx_loot.h"
#include "kx_movement_tuning.h"
#include "kx_survival.h"

// The tuning tables live in EWRAM: IWRAM is kept for code and the stack.
namespace kx
{

tuning_set& movement_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("MOVEMENT", movement_tuning_defaults);
    return result;
}

tuning_set& camera_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("CAMERA", camera_tuning_defaults);
    return result;
}

tuning_set& combat_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("COMBAT", combat_tuning_defaults);
    return result;
}

tuning_set& zombie_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("ZOMBIES", zombie_tuning_defaults);
    return result;
}

tuning_set& inventory_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("INVENTORY", inventory_tuning_defaults);
    return result;
}

tuning_set& survival_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("SURVIVAL", survival_tuning_defaults);
    return result;
}

tuning_set& weapon_tuning(weapon value)
{
    static BN_DATA_EWRAM_BSS tuning_set fists("FISTS", fists_tuning_defaults);
    static BN_DATA_EWRAM_BSS tuning_set knife("KNIFE", knife_tuning_defaults);
    static BN_DATA_EWRAM_BSS tuning_set bat("BAT", bat_tuning_defaults);
    static BN_DATA_EWRAM_BSS tuning_set axe("AXE", axe_tuning_defaults);
    static BN_DATA_EWRAM_BSS tuning_set spear("SPEAR", spear_tuning_defaults);

    switch(value)
    {

    case weapon::KNIFE:
        return knife;

    case weapon::BAT:
        return bat;

    case weapon::AXE:
        return axe;

    case weapon::SPEAR:
        return spear;

    default:
        return fists;
    }
}

}
