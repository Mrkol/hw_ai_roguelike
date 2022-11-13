#pragma once
#include <glm/glm.hpp>
#include <flecs.h>

#include "dungeon.hpp"


namespace dungeon
{

glm::ivec2 find_walkable_tile(flecs::world& ecs);
bool is_tile_walkable(const Dungeon& dd, glm::ivec2 pos);
Dungeon make_dungeon(int width, int height);

};
