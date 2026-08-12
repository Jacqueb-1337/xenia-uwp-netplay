#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace UWP {
// Starts a real WinRT text-edit session. This is required for the Xbox on-screen
// keyboard to deliver text to a custom (non-XAML) control such as ImGui.
void BeginTextInput(const std::string& initial_text = {});
void EndTextInput();
std::string GetTextInput();
bool IsTextInputActive();

// Legacy/fallback character path used by CoreWindow::CharacterReceived.
void ShowKeyboard();
void HandleCharacter(uint32_t keycode);
void HandleBackspace();

extern std::vector<uint32_t> g_char_buffer;
extern std::mutex g_buffer_mutex;
}  // namespace UWP
