#pragma once

#include <behTree.hpp>
#include "components.hpp"


namespace beh_tree
{
  
std::unique_ptr<Node> get_closest(
  flecs::query_builder<const Position> query,
  fu2::function<bool(flecs::entity)> filter,
  std::string_view bb_name);

inline std::unique_ptr<Node> get_closest_enemy(flecs::entity e, std::string_view bb_name)
{
  return get_closest(e.world().query_builder<const Position>().term<Team>(),
    [team = e.get<Team>()->team](flecs::entity candidate)
    {
      return candidate.get<Team>()->team != team;
    },
    bb_name);
}

std::unique_ptr<Node> move_to(std::string_view bb_name, bool flee = false);

} // namespace beh_tree
