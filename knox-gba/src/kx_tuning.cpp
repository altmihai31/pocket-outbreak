#include "kx_tuning.h"

#include "bn_algorithm.h"
#include "bn_assert.h"

namespace kx
{

tuning_set::tuning_set(const char* title, bn::span<const tuning_entry> defaults) :
    _title(title),
    _defaults(defaults)
{
    BN_ASSERT(defaults.size() <= max_entries, "Too many tuning entries: ", defaults.size());

    reset();
}

void tuning_set::adjust(int index, int steps)
{
    const tuning_entry& entry = _defaults[index];
    bn::fixed value = _values[index] + (entry.step * steps);
    _values[index] = bn::clamp(value, entry.min, entry.max);
}

void tuning_set::reset()
{
    for(int index = 0, limit = _defaults.size(); index < limit; ++index)
    {
        _values[index] = _defaults[index].value;
    }
}

}
