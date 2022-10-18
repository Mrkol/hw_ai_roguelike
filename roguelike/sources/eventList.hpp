#pragma once

#include <unordered_set>
#include <flecs.h>


struct EventList
{
  std::unordered_set<flecs::entity> events;
};
