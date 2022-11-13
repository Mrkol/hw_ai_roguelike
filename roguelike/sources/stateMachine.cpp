#include "stateMachine.hpp"

#include <fstream>

#include "assert.hpp"


struct InactiveSubmachineTag {};

StateMachineTracker::StateMachineTracker(
  flecs::world& world, flecs::entity simulateAiPipeline, flecs::entity transitionPhase)
  : world_{world}
  , transitionPhase_{transitionPhase}
{
  auto postTransition = world_.entity().add(simulateAiPipeline).depends_on(transitionPhase);
  world_.system<EventList>("event cache cleaner")
    .kind(postTransition)
    .each(
      [](flecs::entity, EventList& evts)
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
    
    std::unordered_set<flecs::entity> subStateMachines;
    std::unordered_set<flecs::entity> trackedEvents;
    std::optional<flecs::entity> firstState;
    for (auto stateIt = smDesc.begin(); stateIt != smDesc.end(); ++stateIt)
    {
      auto state = world_.entity(stateIt->first.as<std::string>().c_str());
      if (!firstState.has_value())
        firstState = state;

      if (stateMachineAppliers_.contains(state))
        subStateMachines.emplace(state);

      auto& desc = stateIt->second;
      NG_ASSERT(desc.IsSequence());

      for (std::size_t i = 0; i < desc.size(); ++i)
      {
        auto transition = desc[i];
        const bool isSimple = transition.size() == 1;
        auto tgtStateNode = isSimple ? transition.begin()->first : transition["do"];
        auto predNode = isSimple ? transition.begin()->second : transition["when"];
        std::optional<YAML::Node> subStateNode = std::nullopt;
        if (!isSimple && transition["with"].IsDefined())
          subStateNode = transition["with"];

        auto pred = parseEventExpression(predNode, trackedEvents);
        auto tgtState = world_.entity(tgtStateNode.as<std::string>().c_str());

        NG_ASSERT(subStateNode.has_value() == stateMachineAppliers_.contains(tgtState));

        auto tgtSubState = subStateNode.has_value() ? world_.entity(subStateNode->as<std::string>().c_str())
          : flecs::entity{};

        // System for transitioning
        world_.system<EventList>(fmt::format("SM {}: {} to {} transition", sm.name(), state.name(), tgtState.name()).c_str())
          .kind(transitionPhase_)
          .term(sm, state)
          // Hack for proper synchronization
          .term(tgtState).optional().write()
          .each(
            [state, tgtState, sm, pred = std::move(pred),
              srcSubmachine = stateMachineAppliers_.contains(state),
              dstSubmachine = stateMachineAppliers_.contains(tgtState),
              tgtSubState,
              inactiveState = world_.entity<InactiveSubmachineTag>()]
            (flecs::entity e, EventList& evts)
            {
              if (pred(evts))
              {
                if (srcSubmachine)
                  e.add(state, inactiveState);

                e.add(sm, tgtState);
                
                if (dstSubmachine)
                  e.add(tgtState, tgtSubState);
              }
            });
      }
    }

    stateMachineAppliers_.emplace(sm,
      [this, sm,
        state = std::move(*firstState),
        trackedEvents = std::move(trackedEvents),
        subSms = std::move(subStateMachines)]
      (flecs::entity entity)
      {
        entity.add(sm, state);
        entity.add<EventList>();
        for (auto event : trackedEvents)
          entity.add(event);
        for (auto subSm : subSms)
        {
          stateMachineAppliers_.at(subSm)(entity);
          entity.add(subSm, world_.entity<InactiveSubmachineTag>());
        }
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
