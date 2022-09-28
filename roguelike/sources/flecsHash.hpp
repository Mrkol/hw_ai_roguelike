#pragma once

#include <functional>
#include <flecs.h>


namespace std
{
  template<>
  struct hash<flecs::entity>
  {
    std::size_t operator()(const flecs::entity& e) const
    {
      return std::hash<flecs::entity_t>()(e.id());
    }
  };
}

