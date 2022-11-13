#pragma once

#include <array>

#include <glm/glm.hpp>
#include <function2/function2.hpp>
#include <limits>
#include <optional>
#include <spdlog/fmt/fmt.h>
#include <allegro5/keycodes.h>
#include <allegro5/allegro5.h>


#include "allegro5/allegro_font.h"
#include "gameplay/dungeon/dungeonUtils.hpp"
#include "imgui.h"
#include "stateMachine.hpp"
#include "behTree.hpp"

#include "gameplay/components.hpp"
#include "gameplay/systems.hpp"
#include "gameplay/aiSystems.hpp"
#include "gameplay/entityFactories.hpp"
#include "gameplay/behTreeLibrary.hpp"
#include "gameplay/actions.hpp"
#include "gameplay/dungeon/dungeon.hpp"
#include "gameplay/dungeon/dungeonGenerator.hpp"
#include "gameplay/dungeon/dungeonUtils.hpp"


inline ALLEGRO_COLOR colorToAllegro(uint32_t color)
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
    , simulateAiInfo_{register_ai_systems(world_)}
    , smTracker_{world_, simulateAiInfo_.simulateAiPipieline, simulateAiInfo_.stateTransitionPhase}
    , drawableQuery_{
      world_.query_builder<const Position>()
        .term<const Color>().or_()
        .term<const Sprite>().or_()
        .build()}
    , playerQuery_{world_.query_builder<const Position, const NumActions>().term<IsPlayer>().build()}
  {
    smTracker_.load(PROJECT_SOURCE_DIR "/roguelike/resources/monsters.yml");
    auto elfSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/Elf_F_Idle_1.png");
    // auto banditSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/Bandit_Idle_1.png");
    // auto bearSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/Bear_Idle_4.png");
    // auto knightSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/ElvenKnight_Idle_1.png");
    auto gnollSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/GnollBrute_Idle_1.png");
    // auto skull = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/skull.png");

    {
      auto dng = dungeon::make_dungeon(50, 50);
      dungeon::gen_drunk_dungeon(dng.view);
      flecs::entity dngEntity = world_.entity("dungeon")
        .set(std::move(dng));

      create_dmap(world_, "dist_to_player", dngEntity,
        world_.query_builder<const Position>().term<IsPlayer>().build(),
        [](float)
        {
          return 1;
        });

      create_dmap(world_, "dist_to_player_short", dngEntity,
        world_.query_builder<const Position>().term<IsPlayer>().build(),
        [](float d)
        {
          return d > 4 ? 0 : 1; // non-linear weights
        });
    }

    create_player(world_, dungeon::find_walkable_tile(world_))
      .set<Sprite>({elfSprite});


    {
      struct UpdateHealthToBb{};
      flecs::entity mob = create_monster(world_, dungeon::find_walkable_tile(world_)).set<Sprite>({gnollSprite});
      mob.set(SmartMovement
        {
          .potential =
            {
              {"dungeon::dist_to_player", 1.f, "hp_high"},
              {"dungeon::dist_to_player_short", -1.f, "hp_high"},
              {"dungeon::dist_to_player", 1.f, "", -1},
            }
        })
        .add<UpdateHealthToBb>()
        .set(Blackboard{});

      world_.system<UpdateHealthToBb, Blackboard, const Hitpoints>()
        .each([](UpdateHealthToBb, Blackboard& bb, const Hitpoints& hp)
        {
          bb.set(Blackboard::getId("hp_high"), hp.hitpoints > 30 ? 1.f : 0.f);
        });
    }

    // // I hate initialization
    // auto vec = [](auto... vs)
    //   {
    //     std::unique_ptr<beh_tree::Node> init[] = { std::move(vs)... };
    //     return std::vector(
    //       std::make_move_iterator(std::begin(init)),
    //       std::make_move_iterator(std::end(init)));
    //   };
    // auto pairVec = [](auto... vs)
    //   {
    //     std::pair<std::unique_ptr<beh_tree::Node>, fu2::function<float(const Blackboard&) const>> init[] = { std::move(vs)... };
    //     return std::vector(
    //       std::make_move_iterator(std::begin(init)),
    //       std::make_move_iterator(std::end(init)));
    //   };

    // auto move_to_while_visible = [vec](std::string_view name, bool flee = false, bool inverse = false)
    //   {
    //     return beh_tree::race(vec(
    //       beh_tree::move_to(name, flee),
    //       beh_tree::repeat(
    //         beh_tree::predicate(
    //           [var = Blackboard::getId(name), inverse]
    //           (const Visibility& vis, const Position& pos, const Blackboard& bb)
    //           {
    //             auto tgt = bb.get<flecs::entity>(var);
    //             if (!tgt || !tgt->is_alive())
    //               return false;

    //             auto tgtPos = tgt->get<Position>()->v;
    //             return (glm::length(glm::vec2(pos.v) - glm::vec2(tgtPos)) < vis.visibility) != inverse;
    //           },
    //           beh_tree::succeed())
    //       )
    //     ));
    //   };

    // {
    //   flecs::entity mob = create_monster(world_, 5, 5).set<Sprite>({banditSprite});
    //   mob.set<beh_tree::BehTree>(beh_tree::BehTree(mob,
    //     beh_tree::select(vec(
    //         // Prioritize attacking an enemy, while keeping track of health
    //         beh_tree::race(vec(
    //           beh_tree::sequence(vec(
    //             beh_tree::get_closest_enemy(mob, "enemy"),
    //             move_to_while_visible("enemy")
    //           )),
    //           beh_tree::sequence(vec(
    //             beh_tree::wait_event(world_.entity("hp_low")),
    //             beh_tree::fail()
    //           ))
    //         )),
    //         // If attacking an enemy failed, health was low, flee
    //         beh_tree::sequence(vec(
    //           beh_tree::get_closest_enemy(mob, "enemy"),
    //           move_to_while_visible("enemy", true)
    //         ))
    //       ))));
    // }

    // {
    //   flecs::entity mob = create_monster(world_, dungeon::find_walkable_tile(world_)).set<Sprite>({gnollSprite});
    //   mob.set<beh_tree::BehTree>(beh_tree::BehTree(mob,
    //     beh_tree::select(vec(
    //         // Prioritize looting
    //         beh_tree::sequence(vec(
    //           beh_tree::get_closest(
    //             world_.query_builder<const Position>()
    //               .term<HealAmount>().or_()
    //               .term<PowerupAmount>().or_(),
    //             "buff"),
    //           beh_tree::move_to("buff")
    //         )),
    //         // If no loot was found nearby, attack
    //         beh_tree::sequence(vec(
    //           beh_tree::get_closest_enemy(mob, "enemy"),
    //           move_to_while_visible("enemy")
    //         ))
    //       ))));
    // }

    // {
    //   flecs::entity mob = create_monster(world_, -7, 7).set<Sprite>({gnollSprite});
    //   mob.set<beh_tree::BehTree>(beh_tree::BehTree(mob,
    //     beh_tree::select(vec(
    //         // Prioritize attacking visible enemies
    //         beh_tree::sequence(vec(
    //           beh_tree::get_closest_enemy(mob, "enemy"),
    //           move_to_while_visible("enemy")
    //         )),
    //         // Otherwise, patrol
    //         beh_tree::sequence(vec(
    //           beh_tree::race(vec(
    //             beh_tree::move_to("next_wp"),
    //             // Interrupt patrol if enemy is near
    //             beh_tree::sequence(vec(
    //               beh_tree::wait_event(world_.entity("enemy_near")),
    //               beh_tree::fail()
    //             ))
    //           )),
    //           beh_tree::get_pair_target("next_wp", world_.component<Waypoint>(), "next_wp")
    //         ))
    //       ))));

    //   flecs::entity firstWp = create_patrool_route(world_, "route", skull, std::array
    //     {
    //       glm::ivec2{-5, 5},
    //       glm::ivec2{-5, 10},
    //       glm::ivec2{-10, 5},
    //     });
    //   mob.get_mut<Blackboard>()->set(Blackboard::getId("next_wp"), firstWp);
    // }

    // auto createBear =
    //   [&](int x, int y)
    //   {
    //     struct BearTag {};
    //     flecs::entity mob = create_monster(world_, x, y).set<Sprite>({bearSprite});
    //     mob.set(Visibility{2});
    //     mob.add<BearTag>();
    //     mob.set<beh_tree::BehTree>(beh_tree::BehTree(mob,
    //       beh_tree::select(vec(
    //           // Try to heard onto a target
    //           beh_tree::move_to("enemy"),
    //           // Otherwise try to get a visible enemy and heard onto it
    //           beh_tree::sequence(vec(
    //             beh_tree::get_closest_enemy(mob, "enemy"),
    //             beh_tree::broadcast<flecs::entity>(world_.query_builder<Blackboard>().term<BearTag>(), "enemy")
    //           ))
    //         ))));
    //   };

    // createBear(-7, -7);
    // createBear(-7, -10);
    // createBear(-10, -7);
    // createBear(-10, -10);

    // auto calculate_dist = [](std::string_view from, std::string_view to)
    //   {
    //     return beh_tree::calculate<float>(
    //       [f = Blackboard::getId(from)]
    //       (flecs::entity e, const Blackboard& bb) -> std::optional<float>
    //       {
    //         auto o = bb.get<flecs::entity>(f);
    //         if (!o)
    //           return std::nullopt;
    //         return glm::length(glm::vec2(e.get<Position>()->v - o->get<Position>()->v));
    //       }, to);
    //   };


    // glm::ivec2 ant_colony_pos = dungeon::find_walkable_tile(world_);
    // flecs::entity ant_home = world_.entity()
    //   .set<Position>(Position{ant_colony_pos})
    //   .set<Sprite>(Sprite{skull});

    // auto make_ant =
    //   [&, this]()
    //   {
    //     auto maxVisCalculator = [](flecs::entity e, const Blackboard&)
    //       { return std::optional{e.get<Visibility>()->visibility}; };
    //     flecs::entity mob = create_monster(world_, ant_colony_pos).set<Sprite>({gnollSprite});
    //     mob.set<beh_tree::BehTree>(beh_tree::BehTree(mob,
    //       beh_tree::sequence(vec
    //         ( calculate_dist("home", "home_dist")
    //         , beh_tree::select(vec
    //           ( beh_tree::sequence(vec
    //             ( beh_tree::get_closest_enemy(mob, "enemy")
    //             , calculate_dist("enemy", "enemy_dist")
    //             ))
    //           , beh_tree::calculate<float>(maxVisCalculator, "enemy_dist")
    //           ))
    //         , beh_tree::select(vec
    //           ( beh_tree::sequence(vec
    //             ( beh_tree::get_closest_ally(mob, "ally")
    //             , calculate_dist("ally", "ally_dist")
    //             ))
    //           , beh_tree::calculate<float>(maxVisCalculator, "ally_dist")
    //           ))
    //         , beh_tree::utility_select(pairVec
    //           ( std::make_pair // 1. Wander
    //             ( beh_tree::wander_once()
    //             , [ allyDist = Blackboard::getId("ally_dist")
    //               , enemyDist = Blackboard::getId("enemy_dist")
    //               , homeDist = Blackboard::getId("home_dist")
    //               ]
    //               (const Blackboard& bb)
    //               {
    //                 return 6 - *bb.get<float>(homeDist) - *bb.get<float>(allyDist);
    //               }
    //             )
    //           , std::make_pair // 2. Flee back home
    //             ( beh_tree::move_to_once("home")
    //             , [ allyDist = Blackboard::getId("ally_dist")
    //               , enemyDist = Blackboard::getId("enemy_dist")
    //               , homeDist = Blackboard::getId("home_dist")
    //               ]
    //               (const Blackboard& bb)
    //               {
    //                 return *bb.get<float>(allyDist) + *bb.get<float>(homeDist);
    //               }
    //             )
    //           , std::make_pair // 3. Attack
    //             ( beh_tree::move_to_once("enemy")
    //             , [ allyDist = Blackboard::getId("ally_dist")
    //               , enemyDist = Blackboard::getId("enemy_dist")
    //               , homeDist = Blackboard::getId("home_dist")
    //               ]
    //               (const Blackboard& bb)
    //               {
    //                 return 8 - *bb.get<float>(homeDist) - *bb.get<float>(enemyDist);
    //               }
    //             )
    //           ))
    //         ))
    //       ));
    //     mob.get_mut<Blackboard>()->set(Blackboard::getId("home"), ant_home);
    //     mob.get_mut<Hitpoints>()->hitpoints = 25;
    //   };

    // for (int i = 0; i < 8; ++i)
    // {
    //   make_ant();
    // }

    // smTracker_.addSmToEntity(create_monster(world_, 5, 5).set<Sprite>({banditSprite}), "monster");
    // smTracker_.addSmToEntity(create_monster(world_, 10, -5).set<Sprite>({banditSprite}), "monster");
    // smTracker_.addSmToEntity(create_monster(world_, -5, -5).set<Sprite>({gnollSprite}), "berserker");
    // smTracker_.addSmToEntity(create_monster(world_, -5, 5).set<Sprite>({bearSprite}), "healer");
    //
    // smTracker_.addSmToEntity(create_friend(world_, 1, 1).set<Sprite>({knightSprite}), "knight_healer");


    for (int i = 0; i < 10; ++i)
    {
      create_powerup(world_, dungeon::find_walkable_tile(world_), 10.f);
      create_heal(world_, dungeon::find_walkable_tile(world_), 50.f);
    }



    world_.system<const Position>()
      .term<IsPlayer>()
      .each([this](const Position& pos)
        {
          cameraPosition_ = pos.v;
        });
  }

  void wheel(float z)
  {
    self().camScale = std::exp(z / 10.f);
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
    case ALLEGRO_KEY_SPACE:
      action = ActionType::NOP;
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
      [&runAi](IsPlayer, NumActions& acts)
      {
        if (acts.curActions >= acts.numActions)
        {
          acts.curActions = 0;
          runAi = true;
        }
      });

    if (runAi)
    {
      flecs::log::set_level(1);
      world_.run_pipeline(simulateAiInfo_.simulateAiPipieline);
      flecs::log::set_level(-1);
      world_.run_pipeline(endOfTurnPipeline_);
    }
  }

  glm::vec2 cameraPos() const
  {
    return cameraPosition_;
  }

  void drawGui()
  {
    static auto behTreeQuery = world_.query<beh_tree::BehTree>();

    ImGui::Begin("BehTrees");
    behTreeQuery.each([](flecs::entity e, const beh_tree::BehTree& bt)
      {
        auto name = e.name();
        if (ImGui::TreeNode(name.length() == 0 ? fmt::format("{}", e.id()).c_str() : name.c_str()))
        {
          bt.drawDebug();
          ImGui::TreePop();
        }
      });
    ImGui::End();

    static auto dmapQuery = world_.query<dungeon::dmaps::Dmap>();
    ImGui::Begin("Dmaps");
    dmapQuery.each([](flecs::entity e, dungeon::dmaps::Dmap& dmap)
      {
        ImGui::Checkbox(e.name(), &dmap.debugDraw);
      });
    ImGui::End();
  }

  void draw(fu2::unique_function<glm::vec2(glm::vec2)> project)
  {

    static auto wallSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/wall_mid.png");
    static auto floorSprite = self().loadSprite(PROJECT_SOURCE_DIR "/roguelike/resources/floor_1.png");

    static auto dungeonQuery = world_.query<const dungeon::Dungeon>();

    auto bitmapFor = [&](dungeon::Tile tile)
      {
        switch (tile)
        {
          case dungeon::Tile::Floor:
            return self().getSpriteBitmap(floorSprite);

          case dungeon::Tile::Wall:
            return self().getSpriteBitmap(wallSprite);
        }
      };

    dungeonQuery.each(
      [&](const dungeon::Dungeon& d)
      {
        for (int y = 0; y < d.view.extent(0); ++y)
          for (int x = 0; x < d.view.extent(1); ++x)
          {
            auto min = project({x, y});
            auto max = project(glm::ivec2{x, y} + glm::ivec2{1, 1});

            auto bmp = bitmapFor(d.view(y, x));
            al_draw_scaled_bitmap(
              bmp,
              0, 0, al_get_bitmap_width(bmp), al_get_bitmap_height(bmp),
              min.x, min.y, max.x - min.x, max.y - min.y, ALLEGRO_FLIP_VERTICAL);
          }
      });

    static auto dmapQuery = world_.query<dungeon::dmaps::Dmap>();
    dmapQuery.each([&](dungeon::dmaps::Dmap& dmap)
      {
        if (!dmap.debugDraw)
          return;

        for (int y = 0; y < dmap.view.extent(0); ++y)
          for (int x = 0; x < dmap.view.extent(1); ++x)
          {
            auto min = project({x + 0.1, y + 0.5});

            float value = dmap.view(y, x);
            std::string num = value < dungeon::dmaps::INF ? fmt::format("{:.2f}", value) : "INF";

            al_draw_text(self().getFont(), al_map_rgb(255, 255, 255), min.x, min.y, {},
              num.c_str());
          }
      });

    drawableQuery_.each([&](flecs::entity e, const Position &pos)
      {
        auto min = project(pos.v);
        auto max = project(pos.v + glm::ivec2{1, 1});

        if (auto color = e.get<Color>())
          al_draw_filled_rectangle(min.x, min.y, max.x, max.y, colorToAllegro(color->color));

        if (auto sprite = e.get<Sprite>())
          if (auto bitmap = self().getSpriteBitmap(sprite->id))
            al_draw_scaled_bitmap(
              bitmap,
              0, 0, al_get_bitmap_width(bitmap), al_get_bitmap_height(bitmap),
              min.x, min.y, max.x - min.x, max.y - min.y, ALLEGRO_FLIP_VERTICAL);

        if (auto hp = e.get<Hitpoints>())
          al_draw_text(self().getFont(), al_map_rgb(255, 255, 255), min.x + 10, max.y - 10, 0,
            fmt::format("HP: {}", hp->hitpoints).c_str());
        if (auto dmg = e.get<MeleeDamage>())
          al_draw_text(self().getFont(), al_map_rgb(255, 255, 255), min.x + 10, max.y + 10, 0,
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

  flecs::query<const Position> drawableQuery_;
  flecs::query<const Position, const NumActions> playerQuery_;
};
