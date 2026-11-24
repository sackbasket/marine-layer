// Factory presets and the dice. A preset starts from the defaults (plus the
// default macro routing) and changes a few values.
#pragma once
#include "ParamDefs.h"
#include <vector>
#include <string>
#include <utility>
#include <cstdint>

namespace ml
{
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<int, float>> values;
    std::vector<std::string> macroNames;   // empty = defaults
};

const std::vector<FactoryPreset>& factoryPresets();
// default values for every parameter, including the default macro routing
void defaultValues (float* v);
// macro routing used by most presets (names: Fog, Drift, Pitch, Age, Space, Glow)
const char* const* defaultMacroNames();
// roll new values for the sound parameters (leaves mix, output, memory, hand, macros, matrix alone)
void rollDice (float* v, bool wild, uint32_t seed);
} // namespace ml
