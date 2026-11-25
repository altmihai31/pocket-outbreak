#ifndef KX_FIXTURES_H
#define KX_FIXTURES_H

namespace kx::fixtures
{

// A building's fixtures, picked from its furniture: the first kitchen cabinet (top-left in its layout) is the kitchen
// sink, the second the stove; the first bathroom cabinet is the bathroom sink. Outside buildings (the test map)
// every cabinet is a sink.
enum class fixture
{
    NONE,
    KITCHEN_SINK,
    STOVE,
    BATHROOM_SINK
};

[[nodiscard]] fixture at(int tile_x, int tile_y);

[[nodiscard]] inline bool sink(int tile_x, int tile_y)
{
    fixture value = at(tile_x, tile_y);
    return value == fixture::KITCHEN_SINK || value == fixture::BATHROOM_SINK;
}

// The name shown when it is searched, or nullptr.
[[nodiscard]] const char* name(fixture value);

}

#endif
