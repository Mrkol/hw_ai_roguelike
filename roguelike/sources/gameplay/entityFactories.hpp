#pragma once

#include <flecs.h>

flecs::entity create_monster(flecs::world& world, int x, int y, uint32_t color);
flecs::entity create_player(flecs::world& world, int x, int y);
flecs::entity create_friend(flecs::world& world, int x, int y);
void create_heal(flecs::world& world, int x, int y, float amount);
void create_powerup(flecs::world& world, int x, int y, float amount);
