#include "stateMachine.hpp"

#include <fstream>

#include "assert.hpp"


StateMachineTracker::StateMachineTracker(
  flecs::world& world, flecs::entity simulateAiPipeline, flecs::entity transitionPhase)
  : world_{world}
  , transitionPhase_{transitionPhase}
{
  auto postTransition = world_.entity().add(simulateAiPipeline).depends_on(transitionPhase);
  world_.system<EventList>("event cache cleaner")
    .kind(postTransition)
    .each(
      [](flecs::entity e, EventList& evts)
      {
        evts.events.clear();
      });
}

void StateMachineTracker::load(std::filesystem::path path)
{
  const auto root = YAML::LoadFile(path.string());
  NG_ASSERT(root.IsMap());
  for (auto it = root.begin(); it != root.end(); ++it)
  {
    auto sm = world_.entity(it->first.as<std::string>().c_str());
    auto &smDesc = it->second;

    if (stateMachineAppliers_.contains(sm) || smDesc.size() == 0)
      continue;

    sm.add(flecs::Union);
    
    std::unordered_set<flecs::entity> trackedEvents;
    std::optional<flecs::entity> firstState;
    for (auto stateIt = smDesc.begin(); stateIt != smDesc.end(); ++stateIt)
    {
      auto state = world_.entity(stateIt->first.as<std::string>().c_str());
      if (!firstState.has_value())
        firstState = state;

      auto& desc = stateIt->second;
      NG_ASSERT(desc.IsSequence());

      for (std::size_t i = 0; i < desc.size(); ++i)
      {
        std::unordered_set<flecs::entity> stateTrackedEvents;

        auto pred = parseEventExpression(desc[i].begin()->second, trackedEvents);
        auto tgtState = world_.entity(desc[i].begin()->first.as<std::string>().c_str());

        // System for transitioning
        world_.system<EventList>(fmt::format("SM {}: {} to {} transition", sm.name(), state.name(), tgtState.name()).c_str())
          .kind(transitionPhase_)
          .term(sm, state)
          // Hack for proper synchronization
          .term(tgtState).optional().write()
          .each(
            [state, tgtState, sm, pred = std::move(pred)]
            (flecs::entity e, EventList& evts)
            {
              if (pred(evts))
                e.add(sm, tgtState);
            });
      }
    }

    stateMachineAppliers_.emplace(sm,
      [sm, state = std::move(*firstState), trackedEvents = std::move(trackedEvents)]
      (flecs::entity entity)
      {
        entity.add(sm, state);
        entity.add<EventList>();
        for (auto event : trackedEvents)
          entity.add(event);
      });
  }
}

// TODO: this can be optimized using disjunctive normal form
StateMachineTracker::EventPredicate StateMachineTracker::parseEventExpression(
  const YAML::Node& node, std::unordered_set<flecs::entity>& trackedEvents)
{
  if (node.IsMap())
  {
    NG_ASSERT(node.size() == 1);
    auto type = node.begin()->first.as<std::string>();
    auto& data = node.begin()->second;
    if (type == "not")
    {
      return
        [cond = parseEventExpression(data, trackedEvents)](const EventList& evs)
        {
          return !cond(evs);
        };
    }

    NG_ASSERT(data.IsSequence());
    std::vector<EventPredicate> operands;
    operands.reserve(data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
    {
      operands.emplace_back(parseEventExpression(data[i], trackedEvents));
    }

    if (type == "and")
    {
      return
        [ops = std::move(operands)](const EventList& evs)
        {
          bool result = true;
          for (auto& op : ops)
            result = result && op(evs);
          return result;
        };
    }
    else if (type == "or")
    {
      return
        [ops = std::move(operands)](const EventList& evs)
        {
          bool result = false;
          for (auto& op : ops)
            result = result || op(evs);
          return result;
        };
    }

    NG_PANIC("Invalid event expression!");
  }
  else
  {
    NG_ASSERT(node.IsScalar());
    auto event = world_.entity(node.as<std::string>().c_str());
    trackedEvents.emplace(event);
    return
      [event](const EventList& evs)
      {
        return evs.events.contains(event);
      };
  }
}

void StateMachineTracker::addSmToEntity(flecs::entity entity, const char* sm)
{
  auto it = stateMachineAppliers_.find(world_.entity(sm));
  NG_ASSERT(it != stateMachineAppliers_.end());
  it->second(entity);
}
