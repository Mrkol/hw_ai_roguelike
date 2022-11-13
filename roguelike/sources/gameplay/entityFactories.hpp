#pragma once

#include <span>
#include <string_view>
#include <flecs.h>
#include <glm/glm.hpp>
#include <function2/function2.hpp>

#include <sprite.hpp>
#include "components.hpp"
#include "dungeon/dmaps.hpp"


flecs::entity create_monster(flecs::world& world, glm::ivec2 pos);
flecs::entity create_player(flecs::world& world,  glm::ivec2 pos);
flecs::entity create_friend(flecs::world& world, glm::ivec2 pos);
void create_heal(flecs::world& world, glm::ivec2 pos, float amount);
void create_powerup(flecs::world& world, glm::ivec2 pos, float amount);
// Returns starting point
flecs::entity create_patrool_route(flecs::world& world, std::string_view name,
  SpriteId sprite, std::span<const glm::ivec2> coords);

flecs::entity create_dmap(flecs::world& world,
  std::string_view name,
  flecs::entity dungeon,
  flecs::query<const Position> starting_points,
  fu2::function<dungeon::dmaps::PotentialFuncSig> potential);
