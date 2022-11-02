#include <behTree.hpp>

#include <vector>

#include <assert.hpp>


namespace beh_tree
{

template<class Derived>
struct CompoundNode : Node, INodeCaller
{
  std::vector<std::unique_ptr<Node>> children;

  std::unique_ptr<Node> clone() override
  {
    std::unique_ptr<Derived> result{new Derived};
    result->children.reserve(children.size());
    for (auto& child : children)
      result->children.emplace_back(child->clone());
    return std::move(result);
  }

  void connect(flecs::entity entity, INodeCaller* owner) override
  {
    Node::connect(entity, owner);
    for (auto& child : children)
    {
      child->connect(entity, this);
    }
  }
};

template<class Derived>
struct AdapterNode : Node, INodeCaller
{
  std::unique_ptr<Node> adapted;

  std::unique_ptr<Node> clone() override
  {
    std::unique_ptr<Derived> result{new Derived};
    result->adapted = adapted->clone();
    return std::move(result);
  }

  void connect(flecs::entity entity, INodeCaller* owner) override
  {
    Node::connect(entity, owner);
    adapted->connect(entity, this);
  }
};

std::unique_ptr<Node> sequence(std::vector<std::unique_ptr<Node>> nodes)
{
  struct SequenceNode : CompoundNode<SequenceNode>
  {
    size_t current = 0;

    void execute(RunParams params) override
    {
      NG_ASSERT(current == 0);

      if (children.empty())
      {
        succeed(params);
        return;
      }

      children.front()->execute(params);
    }
    
    void cancel(RunParams params) override
    {
      children[current]->cancel(params);
      current = 0;
    }

    void succeeded(RunParams params, Node* which) override
    {
      NG_ASSERT(which == children[current].get());

      if (++current == children.size())
      {
        current = 0;
        succeed(params);
        return;
      }

      children[current]->execute(params);
    }

    void failed(RunParams params, Node* which) override
    {
      NG_ASSERT(which == children[current].get());

      current = 0;
      fail(params);
    }
  };

  auto result = std::make_unique<SequenceNode>();
  std::move(nodes.begin(), nodes.end(), std::back_inserter(result->children));
  return result;}

std::unique_ptr<Node> select(std::vector<std::unique_ptr<Node>> nodes)
{
  struct SelectNode : CompoundNode<SelectNode>
  {
    size_t current = 0;

    void execute(RunParams params) override
    {
      NG_ASSERT(current == 0);

      if (children.empty())
      {
        fail(params);
        return;
      }

      children.front()->execute(params);
    }
    
    void cancel(RunParams params) override
    {
      children[current]->cancel(params);
      current = 0;
    }

    void succeeded(RunParams params, Node* which) override
    {
      NG_ASSERT(which == children[current].get());
      current = 0;
      succeed(params);
    }

    void failed(RunParams params, Node* which) override
    {
      NG_ASSERT(which == children[current].get());

      if (++current == children.size())
      {
        current = 0;
        fail(params);
        return;
      }

      children[current]->execute(params);
    }
  };
  
  auto result = std::make_unique<SelectNode>();
  std::move(nodes.begin(), nodes.end(), std::back_inserter(result->children));
  return result;
}

std::unique_ptr<Node> parallel(std::vector<std::unique_ptr<Node>> nodes)
{
  struct ParallelNode : CompoundNode<ParallelNode>
  {
    size_t finished = 0;
    std::vector<bool> running;

    void execute(RunParams params) override
    {
      NG_ASSERT(finished == 0);
      if (children.empty())
      {
        succeed(params);
        return;
      }
      
      for (size_t i = 0; i < children.size(); ++i)
      {
        // A node may fail as soon as it is executed
        running[i] = true;
        children[i]->execute(params);
        if (!running[i])
          break;
      }
    }

    void cancelChildren(RunParams params)
    {
      for (size_t i = 0; i < children.size(); ++i)
        if (running[i])
          children[i]->cancel(params);
    }
    
    void cancel(RunParams params) override
    {
      cancelChildren(params);
      finished = 0;
      running.assign(children.size(), false);
    }

    void childFinished(Node* which)
    {
      for (size_t i = 0; i < children.size(); ++i)
      {
        if (children[i].get() == which)
          running[i] = false;         
      }
    }

    void succeeded(RunParams params, Node* which) override
    {
      childFinished(which);

      if (++finished == children.size())
      {
        finished = 0;
        running.assign(children.size(), false);
        succeed(params);
      }
    }

    void failed(RunParams params, Node* which) override
    {
      childFinished(which);

      cancelChildren(params);
      finished = 0;
      running.assign(children.size(), false);
      fail(params);
    }
  };

  auto result = std::make_unique<ParallelNode>();
  result->running.resize(nodes.size());
  std::move(nodes.begin(), nodes.end(), std::back_inserter(result->children));
  return result;
}


std::unique_ptr<Node> race(std::vector<std::unique_ptr<Node>> nodes)
{
  struct RaceNode : CompoundNode<RaceNode>
  {
    std::vector<bool> running;

    void execute(RunParams params) override
    {
      for (size_t i = 0; i < children.size(); ++i)
      {
        running[i] = true;
        children[i]->execute(params);
        if (!running[i])
          break;
      }
    }

    void cancelChildren(RunParams params, Node* except)
    {
      for (size_t i = 0; i < children.size(); ++i)
        if (children[i].get() != except && running[i])
          children[i]->cancel(params);
      running.assign(children.size(), false);
    }
    
    void cancel(RunParams params) override
    {
      cancelChildren(params, nullptr);
    }

    void succeeded(RunParams params, Node* which) override
    {
      cancelChildren(params, which);
      succeed(params);
    }

    void failed(RunParams params, Node* which) override
    {
      cancelChildren(params, which);
      fail(params);
    }
  };

  auto result = std::make_unique<RaceNode>();
  result->running.resize(nodes.size(), false);
  std::move(nodes.begin(), nodes.end(), std::back_inserter(result->children));
  return result;
}

std::unique_ptr<Node> repeat(std::unique_ptr<Node> node)
{
  struct RepeatNode : AdapterNode<RepeatNode>, IActor
  {
    bool running = false;

    void execute(RunParams params) override
    {
      entity_.get([this](ActingNodes& a)
        {
          a.actingNodes.emplace(this);
        });
      running = true;
      adapted->execute(params);
    }

    void act(RunParams params) override
    {
      if (!std::exchange(running, true))
        adapted->execute(params);
    }

    void cancel(RunParams) override
    {
      running = false;
      entity_.get([this](ActingNodes& a)
        {
          a.actingNodes.erase(this);
        });
    }

    void succeeded(RunParams, Node*) override
    {
      running = false;
    }

    void failed(RunParams params, Node*) override
    {
      cancel(params);
      fail(params);
    }
  };

  auto result = std::make_unique<RepeatNode>();
  result->adapted = std::move(node);
  return result;
}

std::unique_ptr<Node> detail::predicate_internal(
  fu2::function<bool(flecs::entity)> pred, std::unique_ptr<Node> execute)
{
  struct PredicateNode final : AdapterNode<PredicateNode>
  {
    fu2::function<bool(flecs::entity)> predicate;

    void execute(RunParams params) override
    {
      if (!predicate(entity_))
      {
        fail(params);
        return;
      }
      adapted->execute(params);
    }

    void cancel(RunParams params) override
    {
      adapted->cancel(params);
    }

    std::unique_ptr<Node> clone() override
    {
      auto result = AdapterNode::clone();
      static_cast<PredicateNode*>(result.get())->predicate = predicate;
      return std::move(result);
    }
    
    void succeeded(RunParams params, Node*) override { succeed(params); }
    void failed(RunParams params, Node*) override { fail(params); }
  };

  auto result = std::make_unique<PredicateNode>();
  result->adapted = std::move(execute);
  result->predicate = std::move(pred);
  return result;
}

std::unique_ptr<Node> wait_event(flecs::entity ev)
{
  struct WaitEventNode final : ActionNode<WaitEventNode>, IActor
  {
    flecs::entity event;

    void connect(flecs::entity entity, INodeCaller* owner) override
    {
      ActionNode::connect(entity, owner);
      entity.add(event);
    }

    void execute(RunParams) override
    {
      entity_.get([this](ReactingNodes& r)
        {
          r.eventReactors.emplace(event, this);
        });
    }

    void act(RunParams params) override
    {
      cancel(params);
      succeed(params);
    }

    void cancel(RunParams) override
    {
      entity_.get([this](ReactingNodes& r)
        {
          auto[b, e] = r.eventReactors.equal_range(event);
          while (b != e && b->second != this) ++b;
          NG_ASSERT(b != e);
          r.eventReactors.erase(b);
        });
    }
  };

  auto result = std::make_unique<WaitEventNode>();
  result->event = ev;
  return result;
}

std::unique_ptr<Node> fail()
{
  struct FailNode final : InstantActionNode<FailNode>
  {
    void execute(RunParams params) override { fail(params); }
  };

  return std::make_unique<FailNode>();
}

std::unique_ptr<Node> succeed()
{
  struct SucceedNode final : InstantActionNode<SucceedNode>
  {
    void execute(RunParams params) override { succeed(params); }
  };

  return std::make_unique<SucceedNode>();
}

std::unique_ptr<Node> get_pair_target(std::string_view bb_from, flecs::entity rel, std::string_view bb_to)
{
  struct GetPairTarget : InstantActionNode<GetPairTarget>
  {
    flecs::entity relation;
    size_t bbFrom;
    size_t bbTo;

    GetPairTarget(std::string_view bb_from, flecs::entity rel, std::string_view bb_to)
      : relation{rel}
      , bbFrom{Blackboard::getId(bb_from)}
      , bbTo{Blackboard::getId(bb_to)}
    {
    }

    void execute(RunParams params) override
    {
      bool failed = false;
      entity_.get([this, &failed](Blackboard& bb)
        {
          auto e = bb.get<flecs::entity>(bbFrom);

          if (!e || !e->is_alive())
          {
            failed = true;
            return;
          }

          auto target = e->target(relation);
      
          if (!target)
          {
            failed = true;
            return;
          }

          bb.set(bbTo, target);
        });
  
      if (failed)
        fail(params);
      else
        succeed(params);
    }
  };

  return std::make_unique<GetPairTarget>(bb_from, rel, bb_to);
}


} // namespace beh_tree
