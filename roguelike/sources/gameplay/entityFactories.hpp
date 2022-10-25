#pragma once

#include <span>
#include <string_view>
#include <flecs.h>
#include <glm/glm.hpp>

#include <sprite.hpp>


flecs::entity create_monster(flecs::world& world, int x, int y);
flecs::entity create_player(flecs::world& world, int x, int y);
flecs::entity create_friend(flecs::world& world, int x, int y);
void create_heal(flecs::world& world, int x, int y, float amount);
void create_powerup(flecs::world& world, int x, int y, float amount);
// Returns starting point
flecs::entity create_patrool_route(flecs::world& world, std::string_view name,
  SpriteId sprite, std::span<const glm::ivec2> coords);
