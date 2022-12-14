#include <behTree.hpp>

#include <vector>
#include <random>

#include <assert.hpp>
#include <imgui.h>


namespace beh_tree
{

template<class Derived>
struct CompoundNode : Node, INodeCaller
{
  std::vector<std::unique_ptr<Node>> children;

  virtual void debugDraw() const override
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(running_ ? ImColor(0, 255, 0) : ImColor(255, 255, 255)));
    bool open = ImGui::TreeNodeEx(fmt::format("{}##{}", typeid(Derived).name(), fmt::ptr(this)).c_str());
    ImGui::PopStyleColor();
    if (open)
    {
      for (auto& child : children)
        child->debugDraw();
      ImGui::TreePop();
    }
  }

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

  virtual void debugDraw() const override
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(running_ ? ImColor(0, 255, 0) : ImColor(255, 255, 255)));
    bool open = ImGui::TreeNodeEx(fmt::format("{}##{}", typeid(Derived).name(), fmt::ptr(this)).c_str());
    ImGui::PopStyleColor();
    if (open)
    {
      adapted->debugDraw();
      ImGui::TreePop();
    }
  }

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

    void executeImpl(RunParams params) override
    {
      NG_ASSERT(current == 0);

      if (children.empty())
      {
        succeed(params);
        return;
      }

      children.front()->execute(params);
    }

    void cancelImpl(RunParams params) override
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
  return result;
}

std::unique_ptr<Node> select(std::vector<std::unique_ptr<Node>> nodes)
{
  struct SelectNode : CompoundNode<SelectNode>
  {
    size_t current = 0;

    void executeImpl(RunParams params) override
    {
      NG_ASSERT(current == 0);

      if (children.empty())
      {
        fail(params);
        return;
      }

      children.front()->execute(params);
    }

    void cancelImpl(RunParams params) override
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

std::unique_ptr<Node> utility_select(std::vector<std::pair<std::unique_ptr<Node>, fu2::function<float(const Blackboard&) const>>> nodes)
{
  static constexpr float PER_EXECUTE_BIAS_INCREASE = 2.0f;
  static constexpr float PER_EXECUTE_BIAS_DECREASE = 1.0f;

  struct SelectNode : CompoundNode<SelectNode>
  {
    std::vector<fu2::function<float(const Blackboard&) const>> utilityFuncs;
    std::vector<float> biases;
    std::unordered_map<std::size_t, float> leftToExecute;
    std::size_t currentlyExecuting = 0;


    virtual void debugDraw() const override
    {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(running_ ? ImColor(0, 255, 0) : ImColor(255, 255, 255)));
      bool open = ImGui::TreeNodeEx(fmt::format("{}##{}", typeid(SelectNode).name(), fmt::ptr(this)).c_str());
      ImGui::PopStyleColor();
      if (open)
      {
        std::vector<float> utilities;
        utilities.reserve(utilityFuncs.size());
        for (auto& func : utilityFuncs)
          utilities.push_back(func(*entity_.get<Blackboard>()));

        for (size_t i = 0; auto& child : children)
        {
          ImGui::Text("Utility: %f - %f = %f",
            utilities[i], biases[i], std::max(0.f, utilities[i] - biases[i]));
          ++i;
          child->debugDraw();
        }
        ImGui::TreePop();
      }
    }

    void executeImpl(RunParams params) override
    {
      NG_ASSERT(leftToExecute.size() == 0);

      if (children.empty())
      {
        fail(params);
        return;
      }

      for (auto& bias : biases)
        bias = bias > PER_EXECUTE_BIAS_DECREASE ? bias - PER_EXECUTE_BIAS_DECREASE : 0;

      leftToExecute.reserve(utilityFuncs.size());
      entity_.get([this]
        (Blackboard& bb)
        {
          for (std::size_t i = 0; auto& func : utilityFuncs)
          {
            leftToExecute.emplace(i, func(bb) + biases[i]);
            ++i;
          }
        });

      children[pickChild()]->execute(params);
    }

    std::size_t pickChild()
    {
      static std::default_random_engine eng;

      std::vector<float> weights;
      for (auto[i, w] : leftToExecute)
        weights.emplace_back(std::max(0.f, w - biases[i]));
      std::discrete_distribution<std::size_t> distr(weights.begin(), weights.end());

      currentlyExecuting = distr(eng);
      biases[currentlyExecuting] += PER_EXECUTE_BIAS_INCREASE;
      leftToExecute.erase(currentlyExecuting);
      return currentlyExecuting;
    }

    void cancelImpl(RunParams params) override
    {
      children[currentlyExecuting]->cancel(params);
      leftToExecute.clear();
    }

    void succeeded(RunParams params, Node* which) override
    {
      NG_ASSERT(which == children[currentlyExecuting].get());
      leftToExecute.clear();
      succeed(params);
    }

    void failed(RunParams params, Node* which) override
    {
      NG_ASSERT(which == children[currentlyExecuting].get());

      if (leftToExecute.empty())
      {
        fail(params);
        return;
      }

      children[pickChild()]->execute(params);
    }
  };

  auto result = std::make_unique<SelectNode>();
  result->children.reserve(nodes.size());
  result->utilityFuncs.reserve(nodes.size());
  result->biases.resize(nodes.size(), 0);
  for (auto&[node, func] : nodes)
  {
    result->children.push_back(std::move(node));
    result->utilityFuncs.push_back(std::move(func));
  }
  return result;
}

std::unique_ptr<Node> parallel(std::vector<std::unique_ptr<Node>> nodes)
{
  struct ParallelNode : CompoundNode<ParallelNode>
  {
    size_t finished = 0;
    std::vector<bool> running;

    void executeImpl(RunParams params) override
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
    
    void cancelImpl(RunParams params) override
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

    void executeImpl(RunParams params) override
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
    
    void cancelImpl(RunParams params) override
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

    void executeImpl(RunParams params) override
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

    void cancelImpl(RunParams) override
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
      cancelImpl(params);
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

    void executeImpl(RunParams params) override
    {
      if (!predicate(entity_))
      {
        fail(params);
        return;
      }
      adapted->execute(params);
    }

    void cancelImpl(RunParams params) override
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

    void executeImpl(RunParams) override
    {
      entity_.get([this](ReactingNodes& r)
        {
          r.eventReactors.emplace(event, this);
        });
    }

    void act(RunParams params) override
    {
      cancelImpl(params);
      succeed(params);
    }

    void cancelImpl(RunParams) override
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
    void executeImpl(RunParams params) override { fail(params); }
  };

  return std::make_unique<FailNode>();
}

std::unique_ptr<Node> succeed()
{
  struct SucceedNode final : InstantActionNode<SucceedNode>
  {
    void executeImpl(RunParams params) override { succeed(params); }
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

    void executeImpl(RunParams params) override
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
