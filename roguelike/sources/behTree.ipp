#pragma once

#include "behTree.hpp"


namespace beh_tree
{

namespace detail
{

std::unique_ptr<Node> predicate_internal(fu2::function<bool(flecs::entity)> pred, std::unique_ptr<Node> execute);

template<class Pred, class = decltype(&Pred::oprator())>
struct PredWrapper;

template<class Pred, class... Args>
struct PredWrapper<Pred, bool(*)(Args...)>
{
  Pred pred;
  mutable bool result;

  void operator()(Args... args) const
  {
    result = pred(args...);
  }
};

} // namespace detail

template<class Pred>
std::unique_ptr<Node> predicate(Pred&& pred, std::unique_ptr<Node> execute)
{
  return predicate_internal(
    [w = detail::PredWrapper{std::move(pred), false}]
    (flecs::entity e) mutable
    {
      w.result = false;
      e.get(w);
      return w.result;
    }, std::move(execute));
}

} // namespace beh_tree
