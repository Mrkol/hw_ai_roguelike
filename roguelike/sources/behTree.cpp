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

  void connect(flecs::entity entity, INodeCaller* owner)
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

  void connect(flecs::entity entity, INodeCaller* owner)
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

    void execute() override
    {
      NG_ASSERT(current == 0);

      if (children.empty())
      {
        succeed();
        return;
      }

      children.front()->execute();
    }
    
    void cancel() override
    {
      children[current]->cancel();
      current = 0;
    }

    void succeeded(Node*) override
    {
      if (++current == children.size())
      {
        succeed();
        current = 0;
        return;
      }

      children[current]->execute();
    }

    void failed(Node*) override
    {
      fail();
      current = 0;
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

    void execute() override
    {
      NG_ASSERT(current == 0);

      if (children.empty())
      {
        fail();
        return;
      }

      children.front()->execute();
    }
    
    void cancel() override
    {
      children[current]->cancel();
      current = 0;
    }

    void succeeded(Node*) override
    {
      succeed();
      current = 0;
    }

    void failed(Node*) override
    {
      if (++current == children.size())
      {
        fail();
        current = 0;
        return;
      }

      children[current]->execute();
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

    void execute() override
    {
      NG_ASSERT(finished == 0);
      if (children.empty())
      {
        succeed();
        return;
      }
      
      running.assign(children.size(), true);
      for (auto& child : children)
      {
        child->execute();
      }
    }

    void cancelChildren()
    {
      for (size_t i = 0; i < children.size(); ++i)
        if (running[i])
          children[i]->cancel();
    }
    
    void cancel() override
    {
      cancelChildren();
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

    void succeeded(Node* which) override
    {
      childFinished(which);

      if (++finished == children.size())
      {
        succeed();
        finished = 0;
        running.assign(children.size(), false);
      }
    }

    void failed(Node* which) override
    {
      childFinished(which);

      cancelChildren();
      fail();
      finished = 0;
      running.assign(children.size(), false);
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
    void execute() override
    {
      for (auto& child : children)
        child->execute();
    }

    void cancelChildren(Node* except)
    {
      for (size_t i = 0; i < children.size(); ++i)
        if (children[i].get() != except)
          children[i]->cancel();
    }
    
    void cancel() override
    {
      cancelChildren(nullptr);
    }

    void succeeded(Node* which) override
    {
      cancelChildren(which);
      succeed();
    }

    void failed(Node* which) override
    {
      cancelChildren(which);
      fail();
    }
  };

  auto result = std::make_unique<RaceNode>();
  std::move(nodes.begin(), nodes.end(), std::back_inserter(result->children));
  return result;
}

std::unique_ptr<Node> detail::predicate_internal(
  fu2::function<bool(flecs::entity)> pred, std::unique_ptr<Node> execute)
{
  struct PredicateNode final : AdapterNode<PredicateNode>
  {
    fu2::function<bool(flecs::entity)> predicate;

    void execute() override
    {
      if (!predicate(entity_))
      {
        fail();
        return;
      }
      adapted->execute();
    }

    void cancel() override {}
    

    std::unique_ptr<Node> clone() override
    {
      auto result = AdapterNode::clone();
      static_cast<PredicateNode*>(result.get())->predicate = predicate;
      return std::move(result);
    }
    
    void succeeded(Node*) override { succeed(); }
    void failed(Node*) override { fail(); }
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

    void execute() override
    {
      auto evs = entity_.get<EventList>();
      // Short-circuit for reacting to multiple events within the same frame
      if (evs->events.contains(event))
      {
        succeed();
        return;
      }
      entity_.get_mut<ReactingNodes>()->eventReactors.emplace(event, this);
    }

    void act() override
    {
      cancel();
      succeed();
    }

    void cancel() override
    {
      auto reactors = entity_.get_mut<ReactingNodes>();
      auto[b, e] = reactors->eventReactors.equal_range(event);
      while (b != e && b->second != this) ++b;
      NG_ASSERT(b != e);
      reactors->eventReactors.erase(b);
    }
  };

  auto result = std::make_unique<WaitEventNode>();
  result->event = ev;
  return result;
}

std::unique_ptr<Node> fail()
{
  struct FailNode final : ActionNode<FailNode>
  {
    void execute() override { fail(); }

    void cancel() override {}
  };

  return std::make_unique<FailNode>();
}


} // namespace beh_tree
