#pragma once

#include <cstdint>

#include <string>
#include <filesystem>

namespace winrt {
class fire_and_forget;
}

namespace xe {
namespace ui {
class Window;
}
}  // namespace xe

namespace UWP {
void StartXenia();
void ExecutePendingFunctionsFromUIThread();
void RegisterXeniaWindow(xe::ui::Window* window);
void UpdateImGuiIO();
void SetFrontendKeyboardKey(uint32_t virtual_key, bool down);
void SetModalNavigationCapture(bool capture);
bool IsModalNavigationCaptured();

void RefreshPaths();
std::vector<std::tuple<std::string, std::string>> GetGames();
std::vector<std::string> GetPaths();
void SetGamePaths(std::vector<std::string> paths);
}