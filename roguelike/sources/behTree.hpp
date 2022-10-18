#pragma once

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
  
class INodeCaller
{
public:
  virtual void succeeded(Node* which) = 0;
  virtual void failed(Node* which) = 0;
};

class IActor
{
public:
  virtual void act() = 0;
};

class Node
{
public:
  virtual void execute() = 0;
  
  // Invariant: can only be called if execute was called
  // but the caller has not received a succeeded/failed
  // signal yet.
  virtual void cancel() = 0;

  // cloning polymorphic hierarchies is a pain
  virtual std::unique_ptr<Node> clone() = 0;


  void succeed() { owner_->succeeded(this); }
  void fail() { owner_->failed(this); }

  // Called exactly once after tree has been initialized
  virtual void connect(flecs::entity entity, INodeCaller* owner) { entity_ = entity; owner_ = owner; };

  virtual ~Node() = default;

protected:
  flecs::entity entity_;
  INodeCaller* owner_{nullptr};
};

struct ActingNodes
{
  std::unordered_set<IActor*> actingNodes;
};

struct ReactingNodes
{
  std::unordered_multimap<flecs::entity, IActor*> eventReactors;
};

// Convenience base class for nodes with simple cloning logic
template<class Derived>
struct ActionNode : Node
{
  std::unique_ptr<Node> clone() override
  {
    return std::make_unique<Derived>(static_cast<const Derived&>(*this));
  }
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
      void succeeded(Node*) override
      {
        e.set([](BehTree& bt)
          {
            bt.running_ = false;
          });
      }

      void failed(Node*) override
      {
        e.set([](BehTree& bt)
          {
            bt.running_ = false;
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

  BehTree copy_to(flecs::entity entity)
  {
    return BehTree(entity, root_->clone());
  }

  void execute()
  {
    if (!std::exchange(running_, true) && root_)
      root_->execute();
  }

private:
  bool running_ = false;
  // Flecs staging requires copies :(
  std::shared_ptr<INodeCaller> callback_;
  std::shared_ptr<Node> root_;
};

std::unique_ptr<Node> sequence(std::vector<std::unique_ptr<Node>> nodes);
std::unique_ptr<Node> select(std::vector<std::unique_ptr<Node>> nodes);
std::unique_ptr<Node> parallel(std::vector<std::unique_ptr<Node>> nodes);
// First child to finish decides the overall result
std::unique_ptr<Node> race(std::vector<std::unique_ptr<Node>> nodes);

// Following unreal engine devs' advice, predicates are better left as adapter nodes
// than as succeed/fail returning nodes. Plus there's no need taint behtree code with
// conceptually different boolean expression trees.
template<class Pred>
std::unique_ptr<Node> predicate(Pred pred, std::unique_ptr<Node> execute);

std::unique_ptr<Node> wait_event(flecs::entity event);
std::unique_ptr<Node> fail();

} // namespace beh_tree

#include <behTree.ipp>
