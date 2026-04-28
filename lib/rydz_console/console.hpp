// Odpowiada za rysowanie konsoli i przechwytywanie wejścia z klawiatury
#pragma once
#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "scripting.hpp"
#include "rydz_scripting/lua_resource.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace console {

inline bool is_console_toggle_char(i32 codepoint) {
  return codepoint == '`' || codepoint == '~';
}

// stan konsoli przechowywany jako zasób ECS, zawiera historię wpisów i aktualne
// wejście użytkownika
struct ConsoleState {
  using T = ecs::Resource;
  bool is_open = false;
  std::string current_input = "";
  int cursor_pos = 0;
  std::vector<std::string> history = {
    "[System] Konsola gotowa. Wpisz komende..."
  };
  std::vector<std::string> command_history;
  int history_index = 0;
  int scroll_offset = 0;
  float backspace_held_time = 0.0f;
  bool backspace_repeating = false;

  // dodanie wpisu do logu konsoli, dzieli wielolinijkowe komunikaty na
  // pojedyncze linie i ogranicza historię do 200 wpisów
  void log(std::string const& msg) {
    std::stringstream ss(msg);
    std::string line;
    while (std::getline(ss, line, '\n')) {
      history.push_back(line);
    }

    if (history.size() > 200)
      history.erase(history.begin(), history.begin() + (history.size() - 200));

    scroll_offset = 0;
  }

  // dodanie komendy do historii, zapobiega duplikatom i aktualizuje indeks
  // historii
  void add_command_to_history(std::string const& cmd) {
    if (cmd.empty())
      return;
    if (command_history.empty() || command_history.back() != cmd) {
      command_history.push_back(cmd);
    }
    history_index = static_cast<int>(command_history.size());
  }
};

inline void ConsoleUpdateSystem(ecs::ResMut<ConsoleState> console,
                                ecs::ResMut<scripting::LuaResource> lua,
                                ecs::ResMut<ecs::Input> input,
                                ecs::Res<ecs::Time> time) {
  // klawisze otwarcia
  std::vector<i32> pressed_chars;
  for (i32 key = GetCharPressed(); key > 0; key = GetCharPressed()) {
    pressed_chars.push_back(key);
  }

  bool toggle_requested =
    input->key_pressed(KEY_GRAVE) || input->key_pressed(KEY_F1);
  for (i32 codepoint : pressed_chars) {
    if (is_console_toggle_char(codepoint)) {
      toggle_requested = true;
      break;
    }
  }

  // przełączanie konsoli, czyszczenie aktualnego wejścia po otwarciu
  if (toggle_requested) {
    console->is_open = !console->is_open;
    if (console->is_open) {
      console->current_input.clear();
      console->cursor_pos = 0;
    }
  }

  if (!console->is_open)
    return;

  // przewijanie konsoli
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    console->scroll_offset += static_cast<int>(wheel) * 3;
  }
  if (IsKeyPressed(KEY_PAGE_UP))
    console->scroll_offset += 10;
  if (IsKeyPressed(KEY_PAGE_DOWN))
    console->scroll_offset -= 10;

  int max_scroll = console->history.empty()
                     ? 0
                     : static_cast<int>(console->history.size()) - 1;
  if (console->scroll_offset < 0)
    console->scroll_offset = 0;
  if (console->scroll_offset > max_scroll)
    console->scroll_offset = max_scroll;

  // zbieranie znaków z ograniczeniami
  for (i32 key : pressed_chars) {
    if (key >= 32 && key <= 125 && !is_console_toggle_char(key)) {
      console->current_input.insert(
        console->current_input.begin() + console->cursor_pos,
        static_cast<char>(key)
      );
      console->cursor_pos++;
    }
  }

  float const REPEAT_DELAY =
    0.4f; // czas przytrzymania przed rozpoczęciem powtarzania
  float const REPEAT_RATE =
    0.05f; // czas między kolejnymi usunięciami podczas powtarzania

  auto do_backspace = [&]() {
    if (console->cursor_pos > 0) {
      console->current_input.erase(console->cursor_pos - 1, 1);
      console->cursor_pos--;
    }
  };

  if (rl::IsKeyDown(KEY_BACKSPACE)) {
    if (rl::IsKeyPressed(KEY_BACKSPACE)) {
      do_backspace();
      console->backspace_held_time = 0.0f;
      console->backspace_repeating = false;
    } else {
      console->backspace_held_time += time->delta_seconds;

      if (!console->backspace_repeating &&
          console->backspace_held_time >= REPEAT_DELAY) {
        console->backspace_repeating = true;
        console->backspace_held_time = 0.0f;
        do_backspace();
      } else if (console->backspace_repeating &&
                 console->backspace_held_time >= REPEAT_RATE) {
        console->backspace_held_time = 0.0f;
        do_backspace();
      }
    }
  } else {
    console->backspace_held_time = 0.0f;
    console->backspace_repeating = false;
  }

  if (rl::IsKeyPressed(KEY_LEFT) && console->cursor_pos > 0) {
    console->cursor_pos--;
  }

  if (rl::IsKeyPressed(KEY_RIGHT) &&
      console->cursor_pos < static_cast<int>(console->current_input.size())) {
    console->cursor_pos++;
  }

  // nawigacja po historii komend
  if (rl::IsKeyPressed(KEY_UP) && !console->command_history.empty()) {
    if (console->history_index > 0) {
      console->history_index--;
    }
    console->current_input = console->command_history[console->history_index];
    console->cursor_pos = static_cast<int>(console->current_input.size());
  }

  if (rl::IsKeyPressed(KEY_DOWN) && !console->command_history.empty()) {
    if (console->history_index <
        static_cast<int>(console->command_history.size()) - 1) {
      console->history_index++;
      console->current_input = console->command_history[console->history_index];
      console->cursor_pos = static_cast<int>(console->current_input.size());
    } else {
      console->history_index =
        static_cast<int>(console->command_history.size());
      console->current_input.clear();
      console->cursor_pos = 0;
    }
  }

  // wykonanie komendy po naciśnięciu Enter, dodanie do historii i logu, obsługa
  // błędów Lua
  if (rl::IsKeyPressed(KEY_ENTER) && !console->current_input.empty()) {
    std::string cmd = console->current_input;
    console->log("> " + cmd);
    console->add_command_to_history(cmd);
    console->current_input.clear();
    console->cursor_pos = 0;

    if (cmd.find('(') == std::string::npos &&
        cmd.find(' ') == std::string::npos) {
      cmd += "()";
    }

    int result = luaL_dostring(lua->L, cmd.c_str());
    if (result != LUA_OK) {
      const char *error_msg = lua_tostring(lua->L, -1);
      console->log("[Blad] " +
                   std::string(error_msg ? error_msg : "Nieznany błąd"));
      lua_pop(lua->L, 1);
    }
  }
}

// system renderujący konsolę
inline void ConsoleRenderSystem(
  ecs::Res<ConsoleState> console, ecs::NonSendMarker
) {
  if (!console->is_open)
    return;

  i32 screen_w = rl::GetScreenWidth();
  i32 console_h = rl::GetScreenHeight() / 3;

  rl::DrawRectangle(0, 0, screen_w, console_h, Fade(ecs::Color::BLACK, 0.85f));

  int y = console_h - 30;
  int current_line = 0;

  for (auto it = console->history.rbegin(); it != console->history.rend();
       ++it) {
    if (current_line < console->scroll_offset) {
      current_line++;
      continue;
    }

    rlDrawText(it->c_str(), 10, y, 20, ecs::Color::RAYWHITE);
    y -= 25;
    current_line++;

    if (y < -30)
      break;
  }

  DrawRectangle(0, console_h, screen_w, 30, Fade(ecs::Color::DARKGRAY, 0.9f));
  std::string display =
    "] " + console->current_input.substr(0, console->cursor_pos) + "|" +
    console->current_input.substr(console->cursor_pos);
  rlDrawText(display.c_str(), 10, console_h + 5, 20, ecs::Color::GREEN);
}

// funkcja pluginu, która inicjalizuje zasób stanu konsoli i dodaje system
// aktualizacji do harmonogramu
inline void console_plugin(ecs::App& app) {
  app.init_resource<ConsoleState>();
  app.add_systems(ecs::ScheduleLabel::Update, ConsoleUpdateSystem);
}
} // namespace engine
