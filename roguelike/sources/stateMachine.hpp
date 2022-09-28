#pragma once

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include <flecs.h>
#include <yaml-cpp/yaml.h>
#include <function2/function2.hpp>

#include "flecsHash.hpp"


class StateMachineTracker
{
public:

  StateMachineTracker(flecs::world& world, flecs::entity simulateAiPipeline, flecs::entity transitionPhase);

  void load(std::filesystem::path path);

  // Even dispatchers should match this component and insert
  // events that occured into the set.
  struct EventList
  {
    std::unordered_set<flecs::entity> events;
  };

  void addSmToEntity(flecs::entity entity, const char* sm);

private:
  flecs::world& world_;
  flecs::entity transitionPhase_;

  using EventPredicate = fu2::unique_function<bool(const EventList&) const>;
  EventPredicate parseEventExpression(const YAML::Node& node, std::unordered_set<flecs::entity>& trackedEvents);

  using StateMachineApplier = fu2::unique_function<void(flecs::entity)>;
  std::unordered_map<flecs::entity, StateMachineApplier> stateMachineAppliers_;

  // Event -> handler(event, entity) (one for each SM)
  std::unordered_multimap<flecs::entity, fu2::unique_function<void(flecs::entity, flecs::entity)>> eventHandlers_;
};
