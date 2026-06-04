#pragma once

#include <cstddef>
#include <cstdint>
#include <bitset>

// Maximum number of entities in the ECS
constexpr std::size_t MAX_ENTITIES = 120000;

// Maximum number of different components
constexpr std::size_t MAX_COMPONENTS = 32;

// Alias types for clarity
using Entity = std::uint32_t;
using ComponentType = std::uint8_t;
using Signature = std::bitset<MAX_COMPONENTS>;