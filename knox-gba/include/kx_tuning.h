#ifndef KX_TUNING_H
#define KX_TUNING_H

#include "bn_array.h"
#include "bn_fixed.h"
#include "bn_span.h"

namespace kx
{

// Builds a bn::fixed from thousandths, so tuning tables need no floating-point literals.
[[nodiscard]] constexpr bn::fixed milli(int thousandths)
{
    return bn::fixed::from_data(((thousandths * 4096) + (thousandths >= 0 ? 500 : -500)) / 1000);
}

// One editable tuning value. Defaults live in constexpr tables; the debug menu edits a runtime copy.
struct tuning_entry
{
    const char* name;
    bn::fixed value;
    bn::fixed min;
    bn::fixed max;
    bn::fixed step;
    bool integer;
};

[[nodiscard]] constexpr tuning_entry tune_int(const char* name, int value, int min, int max, int step = 1)
{
    return tuning_entry{ name, value, min, max, step, true };
}

[[nodiscard]] constexpr tuning_entry tune_milli(const char* name, int value, int min, int max, int step)
{
    return tuning_entry{ name, milli(value), milli(min), milli(max), milli(step), false };
}

class tuning_set
{

public:
    static constexpr int max_entries = 32;

    tuning_set(const char* title, bn::span<const tuning_entry> defaults);

    [[nodiscard]] const char* title() const
    {
        return _title;
    }

    [[nodiscard]] int size() const
    {
        return _defaults.size();
    }

    [[nodiscard]] const tuning_entry& entry(int index) const
    {
        return _defaults[index];
    }

    [[nodiscard]] bn::fixed get(int index) const
    {
        return _values[index];
    }

    [[nodiscard]] bool is_default(int index) const
    {
        return _values[index] == _defaults[index].value;
    }

    void adjust(int index, int steps);

    void reset();

private:
    const char* _title;
    bn::span<const tuning_entry> _defaults;
    bn::array<bn::fixed, max_entries> _values;
};

}

#endif
