#include <vector>
#include "font.hpp"
#include "layer.hpp"
#include "cursored_textbox.hpp"
#include "textwindowpekzep.hpp"
#include "praige_r_dict.h"

struct IMEState {
  std::vector<char32_t> solidified;
  std::vector<char32_t> non_solidified;
  std::vector<std::string> candidates;
  int candidate_index;
  void Render(CursoredTextBox& box);
  void ComputeCandidatesAndStore();
};

bool startsWith(const char* haystack, std::vector<char32_t>& needle) {
  for (int i = 0; i < needle.size() ; i++) {
    if (haystack[i] == '\0') { return false; }
    if (haystack[i] != needle[i]) { return false; }
  }
  return true;
}

void IMEState::ComputeCandidatesAndStore() {
  this->candidates.clear();
  for (PekzepChar *c = dict; c != dict_end; c++) {
    if (startsWith(c->praige, this->non_solidified)) {
      this->candidates.push_back(c->hanzi);
    }
  }
  this->candidate_index = 0;
}

void IMEState::Render(CursoredTextBox& box) {
  box.DrawTextCursor(false);
  int solidified_width = WriteUTF32String(*box.text_window->InnerWriter(), Vector2D<int>{4, 6}, this->solidified.data(), ToColor(0));
  int unsolidified_width = WriteUTF32String(
    *box.text_window->InnerWriter(), 
    Vector2D<int>{4 + 8 * solidified_width, 6}, 
    this->non_solidified.data(), ToColor(0xe916c3)
  );
  box.cursor_index = solidified_width + unsolidified_width;
  box.DrawTextCursor(true);

  WriteUTF32String(*box.text_window->InnerWriter(), Vector2D<int>{4, 6 + 17 + 2}, U"[此] 時 火 車 善 子", ToColor(0));
}

void InputTextWindowPekzep(CursoredTextBox& box, char32_t unicode, uint8_t modifier) {
  /*static*/ 
  IMEState state = {};
  state.solidified.push_back(U'我');
  state.solidified.push_back(U'心');
  state.solidified.push_back(U'口');
  state.non_solidified.push_back(U'k');
  state.non_solidified.push_back(U'a');

  state.Render(box);

  if (unicode == 0) {
    return;
  }

/*
  auto cursor_pos = [&box]() { return Vector2D<int>{4 + 8*box.cursor_index, 6}; };

  const int max_chars = (box.text_window->InnerSize().x - 8) / 8 - 1;
  if (unicode == U'\b' && box.cursor_index > 0) {
    box.DrawTextCursor(false);
    --box.cursor_index;
    FillRectangle(*box.text_window->InnerWriter(), cursor_pos(), {8, 16}, ToColor(0xffffff));
    box.DrawTextCursor(true);
  } else if (unicode >= ' ' && box.cursor_index < max_chars) {
    box.DrawTextCursor(false);
    // TODO: This should fail once we allow inputting a fullwidth character from the keyboard
    WriteUnicodeChar(*box.text_window->InnerWriter(), cursor_pos(), unicode, ToColor(0));
    ++box.cursor_index;
    box.DrawTextCursor(true);
  }
*/
  layer_manager->Draw(box.text_window_layer_id);
}
