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

    void executeImpl(RunParams params) override
    {
      bool error = false;
      auto myBb = this->entity_.template get<Blackboard>();
      targets.each(
        [myBb, this, &error](Blackboard& bb)
        {
          auto other = myBb->template get<T>(bbVariable);
          if (!other)
          {
            error = true;
            return;
          }
          bb.set(bbVariable, *other);
        });

      if (error)
        this->fail(params);
      else
        this->succeed(params);
    }
  };

  return std::make_unique<BroadcastNode>(std::move(query), bb_name);
}

template<class T>
std::unique_ptr<Node> calculate(
  fu2::function<std::optional<T>(flecs::entity, const Blackboard&)> func, std::string_view bb_to)
{
  struct CalculateNode : InstantActionNode<CalculateNode>
  {
    fu2::function<std::optional<T>(flecs::entity, const Blackboard&)> function;
    size_t bbVariable;

    CalculateNode(fu2::function<std::optional<T>(flecs::entity, const Blackboard&)> func, std::string_view bb_name)
      : function{std::move(func)}
      , bbVariable{Blackboard::getId(bb_name)}
    {
    }

    void executeImpl(RunParams params) override
    {
      bool error = false;
      this->entity_.template get(
        [&error, this](Blackboard& bb)
        {
          auto val = function(this->entity_, bb);
          error = !val.has_value();
          if (!error)
            bb.set(bbVariable, *val);
        });

      if (error)
        this->fail(params);
      else
        this->succeed(params);
    }
  };

  return std::make_unique<CalculateNode>(std::move(func), bb_to);
}

} // namespace beh_tree
