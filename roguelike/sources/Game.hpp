#pragma once

#include <glm/glm.hpp>
#include <function2/function2.hpp>
#include <allegro5/keycodes.h>

#include "gameplay/components.hpp"
#include "gameplay/systems.hpp"
#include "gameplay/aiSystems.hpp"
#include "gameplay/entityFactories.hpp"

#include "stateMachine.hpp"


ALLEGRO_COLOR colorToAllegro(uint32_t color)
{
  return al_map_rgba
    ( (color >> 0)  & 0xff
    , (color >> 8)  & 0xff
    , (color >> 16) & 0xff
    , (color >> 24) & 0xff
    );
}

template<class Derived>
class Game
{
 public:
  Game()
    : world_{self().world()}
    , endOfTurnPipeline_{register_systems(world_)}
    , simulateAiInfo_{register_ai_systems(world_, smTracker_)}
    , smTracker_{world_, simulateAiInfo_.simulateAiPipieline, simulateAiInfo_.stateTransitionPhase}
    , drawableQuery_{world_.query_builder<const Position, const Color>().build()}
    , playerQuery_{world_.query_builder<const Position, const NumActions>().term<IsPlayer>().build()}
  {

    smTracker_.load(PROJECT_SOURCE_DIR "/roguelike/resources/monsters.yml");

    create_player(world_, 0, 0);

    
    smTracker_.addSmToEntity(create_monster(world_, 5, 5, 0xffee00ee), "monster");
    smTracker_.addSmToEntity(create_monster(world_, 10, -5, 0xffee00ee), "monster");
    smTracker_.addSmToEntity(create_monster(world_, -5, -5, 0xff111111), "berserker");
    create_monster(world_, -5, 5, 0xff00ff00);
    
    create_powerup(world_, 7, 7, 10.f);
    create_powerup(world_, 10, -6, 10.f);
    create_powerup(world_, 10, -4, 10.f);

    create_heal(world_, -5, -5, 50.f);
    create_heal(world_, -5, 5, 50.f);
    

    world_.system<const Position, NumActions>()
      .term<IsPlayer>()
      .each([this](const Position& pos, NumActions& acts)
        {
          cameraPosition_ = pos.v;
        });
  }

  void keyDown(int key)
  {
    ActionType action = ActionType::NOP;
    switch (key)
    {
    case ALLEGRO_KEY_UP:
      action = ActionType::MOVE_UP;
      break;
    case ALLEGRO_KEY_DOWN:
      action = ActionType::MOVE_DOWN;
      break;
    case ALLEGRO_KEY_LEFT:
      action = ActionType::MOVE_LEFT;
      break;
    case ALLEGRO_KEY_RIGHT:
      action = ActionType::MOVE_RIGHT;
      break;

    default:
      return;
    }

    world_.each([action](IsPlayer, Action& act, NumActions& num)
      {
        act.action = action;
        num.curActions++;
      });

    world_.run_pipeline(endOfTurnPipeline_);

    bool runAi = false;
    world_.each(
      [this, &runAi](IsPlayer, NumActions& acts)
      {
        if (acts.curActions >= acts.numActions)
        {
          acts.curActions = 0;
          runAi = true;
        }
      });
    
    if (runAi)
      world_.run_pipeline(simulateAiInfo_.simulateAiPipieline);
  }

  glm::vec2 cameraPos() const
  {
    return cameraPosition_;
  }

  void draw(fu2::unique_function<glm::vec2(glm::vec2)> project)
  {
    const int bars = 10;
    for (int i = -bars; i <= bars; ++i)
    {
      float x = static_cast<float>(i); 
      auto a = project({-bars, x});
      auto b = project({bars, x});
      al_draw_line(a.x, a.y, b.x, b.y, al_map_rgba(100, 100, 100, 128), 2);
      auto c = project({x, -bars});
      auto d = project({x, bars});
      al_draw_line(c.x, c.y, d.x, d.y, al_map_rgba(100, 100, 100, 128), 2);
    }

    drawableQuery_.each([&](flecs::entity e, const Position &pos, const Color color)
      {
        auto min = project(pos.v);
        auto max = project(pos.v + glm::ivec2{1, 1});
        al_draw_filled_rectangle(min.x, min.y, max.x, max.y, colorToAllegro(color.color));
        if (auto hp = e.get<Hitpoints>())
          al_draw_text(self().getFont(), al_map_rgb(255, 255, 255), min.x + 10, max.y + 10, 0,
            fmt::format("HP: {}", hp->hitpoints).c_str());
        if (auto dmg = e.get<MeleeDamage>())
          al_draw_text(self().getFont(), al_map_rgb(255, 255, 255), min.x + 10, max.y + 30, 0,
            fmt::format("DMG: {}", dmg->damage).c_str());
      });
  }
  
 private:
  Derived& self() { return *static_cast<Derived*>(this); }
  const Derived& self() const { return *static_cast<const Derived*>(this); }

 private:
  flecs::world& world_;
  flecs::entity endOfTurnPipeline_;
  SimulateAiInfo simulateAiInfo_;

  StateMachineTracker smTracker_;

  glm::vec2 cameraPosition_;

  flecs::query<const Position, const Color> drawableQuery_;
  flecs::query<const Position, const NumActions> playerQuery_;
};
