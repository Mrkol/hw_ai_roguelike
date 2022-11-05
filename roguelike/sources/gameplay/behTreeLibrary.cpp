#include "behTreeLibrary.hpp"

#include <random>
#include "actions.hpp"
#include <assert.hpp>


namespace beh_tree
{

std::unique_ptr<Node> get_closest(
  flecs::query_builder<const Position> query,
  fu2::function<bool(flecs::entity)> filter,
  std::string_view bb_name)
{
  struct GetClosestNode : ActionNode<GetClosestNode>
  {
    flecs::query<const Position> query;
    fu2::function<bool(flecs::entity)> filter;
    size_t bbVariable;

    GetClosestNode(
      flecs::query_builder<const Position> q,
      fu2::function<bool(flecs::entity)> f,
      std::string_view bb_name)
      : query{std::move(q).build()}
      , filter{std::move(f)}
      , bbVariable{Blackboard::getId(bb_name)}
    {
    }

    void executeImpl(RunParams params) override
    {
      auto mypos = entity_.get<Position>()->v;
      auto vis = entity_.get<Visibility>();
      float visibility = vis ? vis->visibility : std::numeric_limits<float>::max();

      std::optional<flecs::entity> closest;
      float closest_length = 0;
      query.each([&](flecs::entity e, const Position& pos)
        {
          if (!filter(e))
            return;

          float length = glm::length(glm::vec2(mypos - pos.v));

          if (length > visibility)
            return;

          if (!closest.has_value() || length < closest_length)
          {
            closest = e;
            closest_length = length;
          }
        });

      if (!closest.has_value())
      {
        fail(params);
        return;
      }

      entity_.get([this, closest](Blackboard& bb)
        {
          bb.set(bbVariable, closest.value());
        });
      succeed(params);
    }

    void cancelImpl(RunParams) override {}
  };

  return std::make_unique<GetClosestNode>(query, std::move(filter), bb_name);
}

std::unique_ptr<Node> move_to(std::string_view bb_name, bool flee)
{
  struct MoveToNode : ActionNode<MoveToNode>, IActor
  {
    size_t bbVariable;
    bool inverse;

    MoveToNode(std::string_view bb_name, bool inv)
      : bbVariable{Blackboard::getId(bb_name)}
      , inverse{inv}
    {
    }

    void executeImpl(RunParams) override
    {
      entity_.get([this](ActingNodes& a)
        {
          NG_ASSERT(a.actingNodes.emplace(this).second);
        });
    }

    void act(RunParams params) override
    {
      bool success = false;
      bool error = false;
      entity_.get(
        [this, &success, &error]
        (Action& action, const Blackboard& bb, const Position& pos)
        {
          auto tgtEntity = bb.get<flecs::entity>(bbVariable);

          if (!tgtEntity || !tgtEntity->is_alive())
          {
            error = true;
            return;
          }

          auto tgtPos = tgtEntity->get<Position>();

          if (!tgtPos)
          {
            error = true;
            return;
          }

          auto tgt = tgtPos->v;

          if (pos.v == tgt)
          {
            success = true;
          }
          else
          {
            auto dir = move_towards(pos.v, tgt);
            action.action = inverse ? inverse_move(dir) : dir;
          }
        });

      if (error)
      {
        cancelImpl(params);
        fail(params);
      }
      else if (success)
      {
        cancelImpl(params);
        succeed(params);
      }
    }

    void cancelImpl(RunParams) override
    {
      entity_.get([this](ActingNodes& a)
        {
          NG_ASSERT(a.actingNodes.erase(this) == 1);
        });
    }
  };

  return std::make_unique<MoveToNode>(bb_name, flee);
}

std::unique_ptr<Node> move_to_once(std::string_view bb_name, bool flee)
{
  struct MoveToOnceNode : InstantActionNode<MoveToOnceNode>
  {
    size_t bbVariable;
    bool inverse;

    MoveToOnceNode(std::string_view bb_name, bool inv)
      : bbVariable{Blackboard::getId(bb_name)}
      , inverse{inv}
    {
    }

    void executeImpl(RunParams params) override
    {
      bool error = false;
      entity_.get(
        [this, &error]
        (Action& action, const Blackboard& bb, const Position& pos)
        {
          auto tgtEntity = bb.get<flecs::entity>(bbVariable);

          if (!tgtEntity || !tgtEntity->is_alive())
          {
            error = true;
            return;
          }

          auto tgtPos = tgtEntity->get<Position>();

          if (!tgtPos)
          {
            error = true;
            return;
          }

          auto tgt = tgtPos->v;

          auto dir = move_towards(pos.v, tgt);
          action.action = inverse ? inverse_move(dir) : dir;
        });

      if (error)
        fail(params);
      else
        succeed(params);
    }
  };

  return std::make_unique<MoveToOnceNode>(bb_name, flee);
}

static std::default_random_engine engine;

std::unique_ptr<Node> wander()
{
  struct WanderNode : InstantActionNode<WanderNode>, IActor
  {
    void executeImpl(RunParams) override
    {
      entity_.get([this](ActingNodes& a)
        {
          NG_ASSERT(a.actingNodes.emplace(this).second);
        });
    }

    void act(RunParams) override
    {
      entity_.get(
        [](Action& action)
        {
          static std::uniform_int_distribution<> dir(0, 3);
          static ActionType dirs[]
            {
              ActionType::MOVE_UP, ActionType::MOVE_DOWN,
              ActionType::MOVE_LEFT, ActionType::MOVE_RIGHT
            };
          action.action = dirs[dir(engine)];
        });
    }

    void cancelImpl(RunParams) override
    {
      entity_.get([this](ActingNodes& a)
        {
          NG_ASSERT(a.actingNodes.erase(this) == 1);
        });
    }
  };

  return std::make_unique<WanderNode>();
}

std::unique_ptr<Node> wander_once()
{
  struct WanderOnceNode : InstantActionNode<WanderOnceNode>
  {
    void executeImpl(RunParams params) override
    {
      entity_.get(
        [](Action& action)
        {
          static std::uniform_int_distribution<> dir(0, 3);
          static ActionType dirs[]
            {
              ActionType::MOVE_UP, ActionType::MOVE_DOWN,
              ActionType::MOVE_LEFT, ActionType::MOVE_RIGHT
            };
          action.action = dirs[dir(engine)];
        });

      succeed(params);
    }
  };

  return std::make_unique<WanderOnceNode>();
}

} // namespace beh_tree
