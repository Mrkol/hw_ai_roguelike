#pragma once

#include <behTree.hpp>
#include "components.hpp"


namespace beh_tree
{
  
// Accounts for visibility component if present
std::unique_ptr<Node> get_closest(
  flecs::query_builder<const Position> query,
  fu2::function<bool(flecs::entity)> filter,
  std::string_view bb_name);

inline std::unique_ptr<Node> get_closest(
  flecs::query_builder<const Position> query,
  std::string_view bb_name)
{
  return get_closest(query, [](flecs::entity) { return true; }, bb_name);
}

inline std::unique_ptr<Node> get_closest_enemy(flecs::entity e, std::string_view bb_name)
{
  return get_closest(e.world().query_builder<const Position>().term<Team>(),
    [team = e.get<Team>()->team](flecs::entity candidate)
    {
      return candidate.get<Team>()->team != team;
    },
    bb_name);
}

inline std::unique_ptr<Node> get_closest_ally(flecs::entity e, std::string_view bb_name)
{
  return get_closest(e.world().query_builder<const Position>().term<Team>(),
    [team = e.get<Team>()->team](flecs::entity candidate)
    {
      return candidate.get<Team>()->team == team;
    },
    bb_name);
}

std::unique_ptr<Node> move_to(std::string_view bb_name, bool flee = false);
std::unique_ptr<Node> move_to_once(std::string_view bb_name, bool flee = false);
std::unique_ptr<Node> wander();
std::unique_ptr<Node> wander_once();

} // namespace beh_tree
