#ifndef KX_TIMELINE_H
#define KX_TIMELINE_H

#include "bn_common.h"

namespace kx
{

// The world timeline (spec: World timeline and events; mockups/world-timeline.png): the world starts on 1 July with
// everything working and decays on days rolled per world from the seed (worked out on demand, so nothing needs
// storing): the water goes off on day 10-20, the power on day 14-30.
namespace timeline_tuning
{
    constexpr int water_off_first = 10;
    constexpr int water_off_last = 20;
    constexpr int power_off_first = 14;
    constexpr int power_off_last = 30;

    // Drinking without a tap (spec: water must be boiled after the shutoff, or it causes Sick).
    constexpr int tap_water = 25;               // water points per drink (of 100)
    constexpr int raw_water = 20;
    constexpr int river_sick_percent = 35;      // chance an unboiled drink from a river or pond makes you sick
    constexpr int rain_sick_percent = 10;       // from a rain collector
    constexpr int sick_amount = 30;

    // Fridges (spec: they slow spoilage while powered): food found in a powered fridge has aged this fraction.
    constexpr int fridge_age_percent = 25;
}

namespace timeline
{
    // Day 1 is 1 July. "3 JUL".
    void date_text(char* output, int day);

    [[nodiscard]] int water_off_day();
    [[nodiscard]] int power_off_day();

    [[nodiscard]] inline bool water_on(int day)
    {
        return day < water_off_day();
    }

    [[nodiscard]] inline bool power_on(int day)
    {
        return day < power_off_day();
    }
}

}

#endif
