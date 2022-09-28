#pragma once

#include <flecs.h>


// Returns a pipeline for ending a turn
flecs::entity register_systems(flecs::world& world);
