#pragma once

#include "behTree.hpp"


namespace beh_tree
{

namespace detail
{

std::unique_ptr<Node> predicate_internal(fu2::function<bool(flecs::entity)> pred, std::unique_ptr<Node> execute);

template<class Pred, class = decltype(&Pred::operator())>
struct PredWrapper;

template<class Pred, class L, class... Args>
struct PredWrapper<Pred, bool(L::*)(Args...) const>
{
  Pred pred;

  bool operator()(flecs::entity e) const
  {
    return pred(*e.get<std::decay_t<Args>>()...);
  }
};

} // namespace detail

template<class Pred>
std::unique_ptr<Node> predicate(Pred pred, std::unique_ptr<Node> execute)
{
  return detail::predicate_internal(
    detail::PredWrapper<Pred>{std::move(pred)},
    std::move(execute));
}

template<class T>
std::unique_ptr<Node> broadcast(flecs::query_builder<Blackboard> query, std::string_view bb_name)
{
  struct BroadcastNode : InstantActionNode<BroadcastNode>
  {
    flecs::query<Blackboard> targets;
    size_t bbVariable;

    BroadcastNode(flecs::query_builder<Blackboard> query, std::string_view bb_name)
      : targets{std::move(query).build()}
      , bbVariable{Blackboard::getId(bb_name)}
    {
    }

    void execute(RunParams params) override
    {
      auto myBb = this->entity_.get<Blackboard>();
      targets.each(
        [myBb, this](Blackboard& bb)
        {
          bb.set(bbVariable, myBb->get<T>(bbVariable));
        });
    }
  };

  return std::make_unique<BroadcastNode>(std::move(query), bb_name);
}

} // namespace beh_tree
