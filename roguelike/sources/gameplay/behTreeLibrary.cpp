#include "behTreeLibrary.hpp"

#include "actions.hpp"


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

    void execute() override
    {
      auto mypos = entity_.get<Position>()->v;
      auto bb = entity_.get_mut<Blackboard>();

      std::optional<glm::ivec2> closest;
      float closest_length = 0;
      query.each([&](flecs::entity e, const Position& pos)
        {
          if (!filter(e))
            return;

          float length = glm::length(glm::vec2(mypos - pos.v));
          if (!closest.has_value() || length < closest_length)
          {
            closest = pos.v;
            closest_length = length;
          }
        });

      if (!closest.has_value())
      {
        fail();
        return;
      }

      bb->set(bbVariable, closest.value());
      succeed();
    }

    void cancel() override {}
  };

  return std::make_unique<GetClosestNode>(query, std::move(filter), bb_name);
}

std::unique_ptr<Node> move_to(std::string_view bb_name, bool flee)
{
  struct MoveToNode : ActionNode<MoveToNode>, IActor
  {
    flecs::entity moveSystem;
    size_t bbVariable;
    bool inverse;

    MoveToNode(std::string_view bb_name, bool inv)
      : bbVariable{Blackboard::getId(bb_name)}
      , inverse{inv}
    {
    }

    void execute() override
    {
      entity_.get_mut<ActingNodes>()->actingNodes.emplace(this);
    }

    void act() override
    {
      bool success = false;
      entity_.set(
        [this, &success](Action& action, const Position& pos, Blackboard& bb)
        {
          auto tgt = bb.get<glm::ivec2>(bbVariable);
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
      if (success)
      {
        cancel();
        succeed();
      }
    }

    void cancel() override
    {
      entity_.get_mut<ActingNodes>()->actingNodes.erase(this);
    }
  };

  return std::make_unique<MoveToNode>(bb_name, flee);
}

} // namespace beh_tree
