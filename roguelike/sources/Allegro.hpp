#pragma once

#include <unordered_map>
#include <string>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <imgui.h>
#include <backends/imgui_impl_allegro5.h>

#include "assert.hpp"
#include "util.hpp"
#include "sprite.hpp"


template<class Derived>
class Allegro
{
 public:
  static constexpr int kWidth = 1280;
  static constexpr int kHeight = 720;

  Allegro()
  {
    NG_VERIFY(al_init());
    NG_VERIFY(al_install_keyboard());
    NG_VERIFY(al_install_mouse());
    NG_VERIFY(al_init_primitives_addon());
    NG_VERIFY(al_init_image_addon());

    event_queue_ = {al_create_event_queue(), &al_destroy_event_queue};
    display_ = {al_create_display(kWidth, kHeight), &al_destroy_display};
    font_ = {al_create_builtin_font(), &al_destroy_font};
    draw_timer_ = {al_create_timer(1.f/30.f), &al_destroy_timer};

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplAllegro5_Init(display_.get());

    al_register_event_source(event_queue_.get(), al_get_keyboard_event_source());
    al_register_event_source(event_queue_.get(), al_get_mouse_event_source());
    al_register_event_source(event_queue_.get(), al_get_display_event_source(display_.get()));
    al_register_event_source(event_queue_.get(), al_get_timer_event_source(draw_timer_.get()));

    al_start_timer(draw_timer_.get());
  }

  SpriteId loadSprite(const char* path)
  {
    auto it = spriteIds_.find(path);
    if (it == spriteIds_.end())
    {
      SpriteId id = spriteIds_.size();
      it = spriteIds_.emplace(path, id).first;
      UniquePtr<ALLEGRO_BITMAP> bitmap{al_load_bitmap(path), al_destroy_bitmap};
      NG_ASSERT(bitmap.get() != nullptr);
      spriteBitmaps_.emplace(id, std::move(bitmap));
    }
    return it->second;
  }

  ALLEGRO_BITMAP* getSpriteBitmap(SpriteId id)
  {
    auto it = spriteBitmaps_.find(id);
    return it == spriteBitmaps_.end() ? nullptr : it->second.get();
  }

  ~Allegro()
  {
    ImGui_ImplAllegro5_Shutdown();
    ImGui::DestroyContext();
  }

  void poll()
  {
    while (!al_event_queue_is_empty(event_queue_.get()))
    {
      ALLEGRO_EVENT event;
      al_wait_for_event_timed(event_queue_.get(), &event, 0.f);

      ImGui_ImplAllegro5_ProcessEvent(&event);

      switch(event.type)
      {
        case ALLEGRO_EVENT_TIMER:
          if (event.timer.source == draw_timer_.get())
          {
            redraw_this_frame_ = true;
          }
          break;

        case ALLEGRO_EVENT_MOUSE_AXES:
          if constexpr (requires { self().mouse(event.mouse.x, event.mouse.y); })
          {
            self().mouse(event.mouse.x, event.mouse.y);
          }
          if constexpr (requires { self().wheel(event.mouse.z); })
          {
            self().wheel(event.mouse.z);
          }
          break;

        case ALLEGRO_EVENT_KEY_DOWN:
          if constexpr (requires { self().keyDown(event.keyboard.keycode); })
          {
            self().keyDown(event.keyboard.keycode);
          }
          break;

        case ALLEGRO_EVENT_KEY_UP:
          if constexpr (requires { self().keyUp(event.keyboard.keycode); })
          {
            self().keyUp(event.keyboard.keycode);
          }
          break;

        case ALLEGRO_EVENT_DISPLAY_CLOSE:
          self().close();
          break;
      }
    }
  }

  void draw()
  {
    if (redraw_this_frame_)
    {
      ImGui_ImplAllegro5_NewFrame();
      ImGui::NewFrame();
      self().drawGui();
      ImGui::Render();


      al_clear_to_color(al_map_rgb(0, 0, 0));
      self().draw();
      ImGui_ImplAllegro5_RenderDrawData(ImGui::GetDrawData());
      al_flip_display();

      redraw_this_frame_ = false;
    }
  }

  void stop()
  {
    event_queue_.reset();
    display_.reset();
  }

  ALLEGRO_FONT* getFont() { return font_.get(); }

 private:
  Derived& self() { return *static_cast<Derived*>(this); }
  const Derived& self() const { return *static_cast<const Derived*>(this); }

 private:
  UniquePtr<ALLEGRO_EVENT_QUEUE> event_queue_{nullptr, nullptr};
  UniquePtr<ALLEGRO_DISPLAY> display_{nullptr, nullptr};
  UniquePtr<ALLEGRO_FONT> font_{nullptr, nullptr};
  UniquePtr<ALLEGRO_TIMER> draw_timer_{nullptr, nullptr};

  std::unordered_map<std::string, SpriteId> spriteIds_;
  std::unordered_map<SpriteId, UniquePtr<ALLEGRO_BITMAP>> spriteBitmaps_;

  bool redraw_this_frame_ = false;
};
