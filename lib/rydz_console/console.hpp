// Odpowiada za rysowanie konsoli i przechwytywanie wejścia z klawiatury
#pragma once
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "scripting.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace engine {

inline bool is_console_toggle_char(i32 codepoint) {
  return codepoint == '`' || codepoint == '~';
}

//stan konsoli przechowywany jako zasób ECS, zawiera historię wpisów i aktualne wejście użytkownika
struct ConsoleState {
  using T = ecs::Resource;
  bool is_open = false;
  std::string current_input = "";
  int cursor_pos = 0;
  std::vector<std::string> history = {"[System] Konsola gotowa. Wpisz komende..."};
  std::vector<std::string> command_history;
  int history_index = 0;
  int scroll_offset = 0;

  //dodanie wpisu do logu konsoli, dzieli wielolinijkowe komunikaty na pojedyncze linie i ogranicza historię do 200 wpisów
  void log(const std::string &msg) {
    std::stringstream ss(msg);
    std::string line;
    while (std::getline(ss, line, '\n')) {
      history.push_back(line);
    }

    if (history.size() > 200)
      history.erase(history.begin(), history.begin() + (history.size() - 200));

    scroll_offset = 0;
  }

  //dodanie komendy do historii, zapobiega duplikatom i aktualizuje indeks historii
  void add_command_to_history(const std::string &cmd) {
    if (cmd.empty())
      return;
    if (command_history.empty() || command_history.back() != cmd) {
      command_history.push_back(cmd);
    }
    history_index = static_cast<int>(command_history.size());
  }
};

inline void ConsoleUpdateSystem(ecs::ResMut<ConsoleState> console,
                                ecs::ResMut<LuaResource> lua) {
  //klawisze otwarcia
  std::vector<i32> pressed_chars;
  for (i32 key = GetCharPressed(); key > 0; key = GetCharPressed()) {
    pressed_chars.push_back(key);
  }

  bool toggle_requested = IsKeyPressed(KEY_GRAVE) || IsKeyPressed(KEY_F1); 
  for (i32 codepoint : pressed_chars) {
    if (is_console_toggle_char(codepoint)) {
      toggle_requested = true;
      break;
    }
  }

  //przełączanie konsoli, czyszczenie aktualnego wejścia po otwarciu
  if (toggle_requested) {
    console->is_open = !console->is_open;
    if (console->is_open) {
      console->current_input.clear();
      console->cursor_pos = 0;
    }
  }

  if (!console->is_open)
    return;

  //przewijanie konsoli
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

  //zbieranie znaków z ograniczeniami
  for (i32 key : pressed_chars) {
      if (key >= 32 && key <= 125 && !is_console_toggle_char(key)) {
          console->current_input.insert(
              console->current_input.begin() + console->cursor_pos,
              static_cast<char>(key)
          );
          console->cursor_pos++;
      }
  }

  if (rl::IsKeyPressed(KEY_BACKSPACE) && console->cursor_pos > 0) {
      console->current_input.erase(console->cursor_pos - 1, 1);
      console->cursor_pos--;
  }

  if (rl::IsKeyPressed(KEY_LEFT) && console->cursor_pos > 0) {
      console->cursor_pos--;
  }

  if (rl::IsKeyPressed(KEY_RIGHT) &&
      console->cursor_pos < static_cast<int>(console->current_input.size())) {
      console->cursor_pos++;
  }

  //nawigacja po historii komend
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

  //wykonanie komendy po naciśnięciu Enter, dodanie do historii i logu, obsługa błędów Lua
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

    int result = luaL_dostring(lua->vm, cmd.c_str());
    if (result != LUA_OK) {
		const char* error_msg = lua_tostring(lua->vm, -1);
        console->log("[Blad] " + std::string(error_msg ? error_msg : "Nieznany błąd"));
		lua_pop(lua->vm, 1);
    }
  }
}

//system renderujący konsolę
inline void ConsoleRenderSystem(ecs::Res<ConsoleState> console,
                                ecs::NonSendMarker) {
  if (!console->is_open)
    return;

  i32 screen_w = rl::GetScreenWidth();
  i32 console_h = rl::GetScreenHeight() / 3;

  rl::DrawRectangle(0, 0, screen_w, console_h, Fade(rl::BLACK, 0.85f));

  int y = console_h - 30;
  int current_line = 0;

  for (auto it = console->history.rbegin(); it != console->history.rend();
       ++it) {
    if (current_line < console->scroll_offset) {
      current_line++;
      continue;
    }

    DrawText(it->c_str(), 10, y, 20, rl::RAYWHITE);
    y -= 25;
    current_line++;

    if (y < -30)
      break;
  }

  DrawRectangle(0, console_h, screen_w, 30, Fade(DARKGRAY, 0.9f));
  std::string display = "] " + console->current_input.substr(0, console->cursor_pos) + "|" +
      console->current_input.substr(console->cursor_pos);
  DrawText(display.c_str(), 10, console_h + 5, 20, rl::GREEN);
}

//funkcja pluginu, która inicjalizuje zasób stanu konsoli i dodaje system aktualizacji do harmonogramu
inline void console_plugin(ecs::App &app) {
  app.init_resource<ConsoleState>();
  app.add_systems(ecs::ScheduleLabel::Update, ConsoleUpdateSystem);
}
} // namespace engine
