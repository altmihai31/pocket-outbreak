#include "kx_timeline.h"

#include "kx_hash.h"
#include "kx_sandbox.h"
#include "kx_ui.h"
#include "kx_world.h"

namespace kx::timeline
{

namespace
{
    constexpr const char* month_names[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT",
                                            "NOV", "DEC" };
    constexpr int month_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    constexpr int first_month = 6;      // July

    [[nodiscard]] int rolled_day(uint32_t salt, int first, int last)
    {
        return first + int(hash32(world::seed() ^ salt) % uint32_t(last - first + 1));
    }
}

void date_text(char* output, int day)
{
    int month = first_month;
    int date = day;

    while(date > month_days[month])
    {
        date -= month_days[month];
        month = (month + 1) % 12;
    }

    int length = ui::append_int(output, 0, date);
    length = ui::append(output, length, " ");
    ui::append(output, length, month_names[month]);
}

int water_off_day()
{
    // The sandbox's shutoff (milestone 13); normal is the timeline's own range.
    int setting = sandbox::value(sandbox::setting::SHUTOFF);
    const int* range = sandbox::tuning::water_off[setting];
    return setting == 1 ? rolled_day(0x57415452u, timeline_tuning::water_off_first, timeline_tuning::water_off_last) :
                          rolled_day(0x57415452u, range[0], range[1]);
}

int power_off_day()
{
    int setting = sandbox::value(sandbox::setting::SHUTOFF);
    const int* range = sandbox::tuning::power_off[setting];
    return setting == 1 ? rolled_day(0x504f5752u, timeline_tuning::power_off_first, timeline_tuning::power_off_last) :
                          rolled_day(0x504f5752u, range[0], range[1]);
}

}
