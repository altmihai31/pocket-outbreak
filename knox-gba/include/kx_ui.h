#ifndef KX_UI_H
#define KX_UI_H

#include "kx_items.h"

namespace kx::ui
{

// Text building without printf (no libc in the ROM). Each returns the new length and keeps `output` terminated.
int append(char* output, int length, const char* text);
int append_int(char* output, int length, int value);
int append_signed(char* output, int length, int value);
int append_weight(char* output, int length, int weight);        // 10 g units -> "11.2"

// Item stats line for the info box, e.g. "FOOD +35 · WATER -5 · 0.4 KG" (mockup 03).
void item_stats(char* output, const item_stack& stack, int today);

// Shoulder button names as printed on the Miyoo Mini (the main target), whose L1 and R1 are the GBA's L and R.
// A, B, Select and Start keep their names.
namespace button
{
    constexpr const char* l = "L1";
    constexpr const char* r = "R1";
    constexpr const char* l_r = "L1/R1";
}

}

#endif
