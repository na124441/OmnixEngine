#pragma once

#include <cstdint>

// Standardize ComponentTypeID to uint8_t matching ECSConfig.h
using ComponentTypeID = uint8_t;

// Component type identifiers (0-based indexing)
constexpr ComponentTypeID HEALTH_COMPONENT = 0;
constexpr ComponentTypeID TRANSFORM_COMPONENT = 1;
constexpr ComponentTypeID PHYSICS_COMPONENT = 2;
constexpr ComponentTypeID INVENTORY_COMPONENT = 3;
constexpr ComponentTypeID MESH_RENDERER_COMPONENT = 4;
constexpr ComponentTypeID CAMERA_COMPONENT = 5;
constexpr ComponentTypeID COLLIDER_COMPONENT = 6;
constexpr ComponentTypeID PLAYER_CONTROLLER_COMPONENT = 7;
constexpr ComponentTypeID TAG_COMPONENT = 8;
constexpr ComponentTypeID LAYER_COMPONENT = 9;
constexpr ComponentTypeID AUDIO_SOURCE_COMPONENT = 10;
constexpr ComponentTypeID ANIMATOR_COMPONENT = 11;
constexpr ComponentTypeID SCRIPT_COMPONENT = 12;
constexpr ComponentTypeID NAME_COMPONENT = 13;
constexpr ComponentTypeID RENDERABLE_MESH_COMPONENT = 14;
constexpr ComponentTypeID MATERIAL_COMPONENT = 15;
constexpr ComponentTypeID STATIC_BODY_COMPONENT = 16;
constexpr ComponentTypeID BOX_COLLIDER_COMPONENT = 17;
constexpr ComponentTypeID SPHERE_COLLIDER_COMPONENT = 18;
constexpr ComponentTypeID CAPSULE_COLLIDER_COMPONENT = 19;
constexpr ComponentTypeID PLAYER_START_COMPONENT = 20;
constexpr ComponentTypeID CHARACTER_CONTROLLER_COMPONENT = 21;
constexpr ComponentTypeID INPUT_COMPONENT = 22;
constexpr ComponentTypeID TRIGGER_COMPONENT = 23;
constexpr ComponentTypeID INTERACTABLE_COMPONENT = 24;
constexpr ComponentTypeID DIRECTIONAL_LIGHT_COMPONENT = 26;
constexpr ComponentTypeID POINT_LIGHT_COMPONENT = 27;
constexpr ComponentTypeID AMBIENT_LIGHT_COMPONENT = 28;
constexpr ComponentTypeID SPOT_LIGHT_COMPONENT = 29;

