#pragma once

#include "assert.hpp"
#include "imgui.h"
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include <function2/function2.hpp>

#include <flecsHash.hpp>
#include <blackboard.hpp>
#include <eventList.hpp>


namespace beh_tree
{

// Note: the design is slow af because we store a new tree hierarchy for each actor.
// This can be easily mitigated by storing entity -> data maps inside nodes, but
// that's too much work for a homework assignment.

class Node;
class IActor;
class BehTree;

struct ActingNodes
{
  std::unordered_set<IActor*> actingNodes;
};

struct ReactingNodes
{
  std::unordered_multimap<flecs::entity, IActor*> eventReactors;
};

struct RunParams {};

class INodeCaller
{
public:
  virtual void succeeded(RunParams params, Node* which) = 0;
  virtual void failed(RunParams params, Node* which) = 0;

  virtual ~INodeCaller() = default;
};

class IActor
{
public:
  virtual void act(RunParams params) = 0;
};

class Node
{
public:
  void execute(RunParams params)
  {
    start();
    executeImpl(params);
  }

  void cancel(RunParams params)
  {
    stop();
    cancelImpl(params);
  }

  virtual void debugDraw() const = 0;

  // Invariant: can only be called if execute was called
  // but the caller has not received a succeeded/failed
  // signal yet.

  // cloning polymorphic hierarchies is a pain
  virtual std::unique_ptr<Node> clone() = 0;

  // Called exactly once after tree has been initialized
  virtual void connect(flecs::entity entity, INodeCaller* owner) { entity_ = entity; owner_ = owner; };

  virtual ~Node() = default;

protected:
  virtual void executeImpl(RunParams params) = 0;
  virtual void cancelImpl(RunParams params) = 0;

  void succeed(RunParams params) { stop(); owner_->succeeded(params, this); }
  void fail(RunParams params) { stop(); owner_->failed(params, this); }

private:
  void start() { NG_ASSERT(!std::exchange(running_, true)); }
  void stop() { NG_ASSERT(std::exchange(running_, false)); }

protected:
  bool running_ = false;
  flecs::entity entity_;
  INodeCaller* owner_{nullptr};
};

// Convenience base class for nodes with simple cloning logic
template<class Derived>
struct ActionNode : Node
{
  virtual void debugDraw() const override
  {
    ImGui::TextColored(ImVec4(running_ ? ImColor(0, 255, 0) : ImColor(255, 255, 255)),
      "%s##%p", typeid(Derived).name(), this);
  }

  std::unique_ptr<Node> clone() override
  {
    return std::make_unique<Derived>(static_cast<const Derived&>(*this));
  }
};

template<class Derived>
struct InstantActionNode : ActionNode<Derived>
{
  void cancelImpl(RunParams) override {}
};


class BehTree
{
public:
  BehTree() = default;

  BehTree(flecs::entity entity, std::unique_ptr<Node> root)
    : root_{std::move(root)}
  {
    struct Callback : INodeCaller
    {
      void succeeded(RunParams, Node*) override
      {
        e.get([](BehTree& tree)
          {
            tree.running_ = false;
          });
      }

      void failed(RunParams, Node*) override
      {
        e.get([](BehTree& tree)
          {
            tree.running_ = false;
          });
      }

      flecs::entity e;
    };

    auto cb = std::make_unique<Callback>();
    cb->e = entity;
    callback_ = std::move(cb);

    root_->connect(entity, callback_.get());
    entity.set<EventList>({})
      .set<Blackboard>({})
      .set<ActingNodes>({})
      .set<ReactingNodes>({});
  }

  BehTree(BehTree&&) = default;
  BehTree& operator=(BehTree&&) = default;
  BehTree(const BehTree&) = default;
  BehTree& operator=(const BehTree&) = default;

  void drawDebug() const
  {
    root_->debugDraw();
  }

  BehTree copy_to(flecs::entity entity)
  {
    return BehTree(entity, root_->clone());
  }

  void execute(RunParams params)
  {
    if (!std::exchange(running_, true) && root_)
      root_->execute(params);
  }

private:
  bool running_ = false;
  // Flecs staging requires copies :(
  std::shared_ptr<INodeCaller> callback_;
  std::shared_ptr<Node> root_;
};

std::unique_ptr<Node> sequence(std::vector<std::unique_ptr<Node>> nodes);
std::unique_ptr<Node> select(std::vector<std::unique_ptr<Node>> nodes);
std::unique_ptr<Node> utility_select(std::vector<std::pair<std::unique_ptr<Node>, fu2::function<float(const Blackboard&) const>>> nodes);
std::unique_ptr<Node> parallel(std::vector<std::unique_ptr<Node>> nodes);
// First child to finish decides the overall result
std::unique_ptr<Node> race(std::vector<std::unique_ptr<Node>> nodes);
// Repeats a node until it fails
std::unique_ptr<Node> repeat(std::unique_ptr<Node> node);

// Following unreal engine devs' advice, predicates are better left as adapter nodes
// than as succeed/fail returning nodes. Plus there's no need taint behtree code with
// conceptually different boolean expression trees.
template<class Pred>
std::unique_ptr<Node> predicate(Pred pred, std::unique_ptr<Node> execute);

std::unique_ptr<Node> wait_event(flecs::entity event);
std::unique_ptr<Node> fail();
std::unique_ptr<Node> succeed();
std::unique_ptr<Node> get_pair_target(std::string_view bb_from, flecs::entity rel, std::string_view bb_to);

template<class T>
std::unique_ptr<Node> broadcast(flecs::query_builder<Blackboard> query, std::string_view bb_name);
template<class T>
std::unique_ptr<Node> calculate(fu2::function<std::optional<T>(flecs::entity, const Blackboard&)> func, std::string_view bb_to);

} // namespace beh_tree

#include <behTree.ipp>
