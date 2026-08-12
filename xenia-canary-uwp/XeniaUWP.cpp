#include "XeniaUWP.h"

#include "UWPUtil.h"
#include "WinRTKeyboard.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>

#include "windowed_app_context_uwp.h"
#include "surface_uwp.h"
#include "window_uwp.h"

#include "third_party/imgui/imgui.h"

#include "xenia/emulator.h"
#include "xenia/base/filesystem.h"
#include "xenia/ui/windowed_app.h"
#include "xenia/base/cvar.h"
#include "xenia/base/clock.h"
#include "xenia/base/logging.h"
#include "xenia/ui/window.h"
#include "xenia/ui/d3d12/d3d12_provider.h"
#include "xenia/gpu/d3d12/d3d12_graphics_system.h"
#include "xenia/hid/xinput/xinput_hid.h"
#include "xenia/hid/nop/nop_hid.h"
#include "xenia/apu/xaudio2/xaudio2_audio_system.h"
#include "xenia/config.h"
#include "xenia/base/main_win.h"
#include "xenia/vfs/devices/disc_zarchive_device.h"

using namespace xe;
using namespace xe::hid;

DECLARE_string(gamepaths);
DEFINE_string(gamepaths, "", "Paths the frontend will search for games.",
              "General");

static std::unique_ptr<ui::WindowedApp> app = nullptr;
static std::unique_ptr<ui::UWPWindowedAppContext> app_context = nullptr;
static ui::Window* s_window;
static Emulator* s_emulator;
static std::vector<std::string> s_paths;
static std::vector<std::tuple<std::string, std::string>> s_games;
static std::vector<std::string> s_scanned_paths;
static bool s_modal_navigation_capture = false;

namespace {
constexpr uint64_t kAnalogNavInitialDelayMs = 275;
constexpr uint64_t kAnalogNavRepeatIntervalMs = 115;

std::string NormalizeScannedPath(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    normalized = path.lexically_normal();
  }

  std::string normalized_string = xe::path_to_utf8(normalized);
  std::replace(normalized_string.begin(), normalized_string.end(), '/', '\\');
  std::transform(normalized_string.begin(), normalized_string.end(),
                 normalized_string.begin(), [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized_string;
}

bool HasScannedDirectory(const std::string& normalized_path) {
  return std::find(s_scanned_paths.cbegin(), s_scanned_paths.cend(),
                   normalized_path) != s_scanned_paths.cend();
}

bool IsConfiguredStorageRoot(const std::filesystem::path& path) {
  const std::string normalized_path = NormalizeScannedPath(path);
  for (const char* var_name : {"content_root", "cache_root", "storage_root"}) {
    auto it = cvar::ConfigVars->find(var_name);
    if (it == cvar::ConfigVars->end()) {
      continue;
    }
    auto* configured_path =
        dynamic_cast<cvar::ConfigVar<std::filesystem::path>*>(it->second);
    if (!configured_path) {
      continue;
    }
    const auto root = configured_path->GetTypedConfigValue();
    if (!root.empty() && NormalizeScannedPath(root) == normalized_path) {
      return true;
    }
  }
  return false;
}

bool AddGameEntry(const std::filesystem::path& path, const std::string& name) {
  const std::string normalized_path = NormalizeScannedPath(path);
  auto existing = std::find_if(
      s_games.cbegin(), s_games.cend(), [&](const auto& game) {
        return NormalizeScannedPath(std::get<0>(game)) == normalized_path;
      });
  if (existing != s_games.cend()) {
    return false;
  }

  s_games.push_back({path.string(), name});
  XELOGI("[UWP] Game library entry: {} -> {}", path.string(), name);
  return true;
}

enum class NavDirection { kLeft = 0, kRight, kUp, kDown };

struct NavRepeatState {
  bool active = false;
  bool repeating = false;
  uint64_t start_time_ms = 0;
  uint64_t last_emit_time_ms = 0;
};

std::array<NavRepeatState, 4> g_nav_repeat_states;
uint16_t g_previous_ui_buttons = 0;
bool g_previous_ui_lt = false;
bool g_previous_ui_rt = false;

bool UpdateNavRepeatState(NavDirection direction, bool active,
                          uint64_t now_ms) {
  auto& state = g_nav_repeat_states[static_cast<size_t>(direction)];

  if (!active) {
    state = {};
    return false;
  }

  if (!state.active) {
    state.active = true;
    state.start_time_ms = now_ms;
    state.last_emit_time_ms = now_ms;
    return true;
  }

  if (!state.repeating) {
    if (now_ms - state.start_time_ms >= kAnalogNavInitialDelayMs) {
      state.repeating = true;
      state.last_emit_time_ms = now_ms;
      return true;
    }
    return false;
  }

  if (now_ms - state.last_emit_time_ms >= kAnalogNavRepeatIntervalMs) {
    state.last_emit_time_ms = now_ms;
    return true;
  }

  return false;
}

void ResetFrontendInputStates() {
  for (auto& state : g_nav_repeat_states) {
    state = {};
  }
  g_previous_ui_buttons = 0;
  g_previous_ui_lt = false;
  g_previous_ui_rt = false;
}

}  // namespace

void UWP::SetModalNavigationCapture(bool capture) {
  if (s_modal_navigation_capture == capture) {
    return;
  }
  s_modal_navigation_capture = capture;
  XELOGI("UWP modal navigation capture: {}", capture ? "on" : "off");
}

bool UWP::IsModalNavigationCaptured() { return s_modal_navigation_capture; }

void UWP::StartXenia() {
  app_context = std::make_unique<ui::UWPWindowedAppContext>();
  app = xe::ui::GetWindowedAppCreator()(*app_context.get());

  xe::InitializeWin32App(app->GetName());

  if (app->OnInitialize()) {
    RefreshPaths();
    // to-do, remodel this so it doesn't instantly shutdown.
    //app->InvokeOnDestroy();
  }

  //xe::ShutdownWin32App();
}

void UWP::ExecutePendingFunctionsFromUIThread() {
  app_context->ExecutePendingFunctionsFromUIThread();

  if (s_window) {
    app_context->CallInUIThread([=]() { s_window->RequestPaint(); });
  }
}

void UWP::RegisterXeniaWindow(xe::ui::Window* window) { s_window = window; }

void UWP::UpdateImGuiIO() {
  static bool logged_input_entry = false;
  if (!logged_input_entry) {
    XELOGI("[UWP] Frontend input pump reached");
    logged_input_entry = true;
  }

  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Backspace, false);
  io.AddKeyEvent(ImGuiKey_Enter, false);

  {
    std::unique_lock lk(UWP::g_buffer_mutex);
    for (uint32_t c : UWP::g_char_buffer) {
      io.AddInputCharacter(c);

      if (c == '\b') {
        io.AddKeyEvent(ImGuiKey_Backspace, true);
      } else if (c == '\r') {
        io.AddKeyEvent(ImGuiKey_Enter, true);
      }
    }
    UWP::g_char_buffer.clear();
  }

  // Called from ImGuiDrawer::Draw after the UWP frontend ImGui context is
  // active. This is the sole controller-to-ImGui path on WinRT.
  auto driver = static_cast<xe::ui::UWPWindow*>(s_window)->xinputdriver();
  if (!driver) {
    static bool logged_missing_driver = false;
    if (!logged_missing_driver) {
      XELOGW("[UWP] Frontend input pump has no XInput driver");
      logged_missing_driver = true;
    }
    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    return;
  }

  // Xbox may assign the active controller to any XInput user slot. Prefer a
  // controller that currently has activity, otherwise use the first connected
  // one so release events continue to reach ImGui.
  hid::X_INPUT_STATE state = {};
  hid::X_INPUT_STATE fallback_state = {};
  bool have_state = false;
  bool have_fallback = false;
  uint32_t selected_user = 0xFFFFFFFFu;
  uint32_t fallback_user = 0xFFFFFFFFu;
  constexpr int16_t kActivityStickDeadzone = 6000;
  for (uint32_t user_index = 0; user_index < 4; ++user_index) {
    hid::X_INPUT_STATE candidate = {};
    if (driver->GetState(user_index, &candidate) != X_STATUS_SUCCESS) {
      continue;
    }
    if (!have_fallback) {
      fallback_state = candidate;
      fallback_user = user_index;
      have_fallback = true;
    }
    const auto& pad = candidate.gamepad;
    const bool active =
        pad.buttons != 0 || pad.left_trigger > 30 || pad.right_trigger > 30 ||
        std::abs(pad.thumb_lx) > kActivityStickDeadzone ||
        std::abs(pad.thumb_ly) > kActivityStickDeadzone ||
        std::abs(pad.thumb_rx) > kActivityStickDeadzone ||
        std::abs(pad.thumb_ry) > kActivityStickDeadzone;
    if (active) {
      state = candidate;
      selected_user = user_index;
      have_state = true;
      break;
    }
  }
  if (!have_state && have_fallback) {
    state = fallback_state;
    selected_user = fallback_user;
    have_state = true;
  }
  if (!have_state) {
    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    return;
  }

  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  static uint32_t last_logged_user = 0xFFFFFFFFu;
  if (selected_user != last_logged_user) {
    XELOGI("[UWP] Frontend controller using XInput user {}", selected_user);
    last_logged_user = selected_user;
  }

  const auto& gamepad = state.gamepad;
  const uint16_t buttons = gamepad.buttons;
  if (buttons) {
    static uint16_t last_logged_buttons = 0;
    if (buttons != last_logged_buttons) {
      XELOGI("[UWP] Frontend controller buttons: 0x{:04X}", buttons);
      last_logged_buttons = buttons;
    }
  }

  // Feed normal held state into ImGui. With the old RequestPaintImpl call
  // removed there is now only one gamepad event source, so ImGui can perform
  // its own de-duplication/repeat handling correctly.
  io.AddKeyEvent(ImGuiKey_GamepadFaceDown,
                 (buttons & X_INPUT_GAMEPAD_A) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceRight,
                 (buttons & X_INPUT_GAMEPAD_B) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, false);
  io.AddKeyEvent(ImGuiKey_F12, (buttons & X_INPUT_GAMEPAD_X) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceUp,
                 (buttons & X_INPUT_GAMEPAD_Y) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadStart,
                 (buttons & X_INPUT_GAMEPAD_START) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadBack,
                 (buttons & X_INPUT_GAMEPAD_BACK) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL1,
                 !s_modal_navigation_capture &&
                     (buttons & X_INPUT_GAMEPAD_LEFT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR1,
                 !s_modal_navigation_capture &&
                     (buttons & X_INPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL3,
                 (buttons & X_INPUT_GAMEPAD_LEFT_THUMB) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR3,
                 (buttons & X_INPUT_GAMEPAD_RIGHT_THUMB) != 0);

  const int16_t kStickNavDeadzone = X_INPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
  const bool nav_left = gamepad.thumb_lx <= -kStickNavDeadzone ||
                        (buttons & X_INPUT_GAMEPAD_DPAD_LEFT) != 0;
  const bool nav_right = gamepad.thumb_lx >= kStickNavDeadzone ||
                         (buttons & X_INPUT_GAMEPAD_DPAD_RIGHT) != 0;
  const bool nav_up = gamepad.thumb_ly >= kStickNavDeadzone ||
                      (buttons & X_INPUT_GAMEPAD_DPAD_UP) != 0;
  const bool nav_down = gamepad.thumb_ly <= -kStickNavDeadzone ||
                        (buttons & X_INPUT_GAMEPAD_DPAD_DOWN) != 0;
  io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, nav_left);
  io.AddKeyEvent(ImGuiKey_GamepadDpadRight, nav_right);
  io.AddKeyEvent(ImGuiKey_GamepadDpadUp, nav_up);
  io.AddKeyEvent(ImGuiKey_GamepadDpadDown, nav_down);

  constexpr float kStickDeadzone = 8000.0f / 32767.0f;
  const float rx = gamepad.thumb_rx / 32767.0f;
  const float ry = gamepad.thumb_ry / 32767.0f;
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft, rx < -kStickDeadzone,
                       rx < 0 ? -rx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, rx > kStickDeadzone,
                       rx > 0 ? rx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp, ry > kStickDeadzone,
                       ry > 0 ? ry : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, ry < -kStickDeadzone,
                       ry < 0 ? -ry : 0.0f);

  const float lt = gamepad.left_trigger / 255.0f;
  const float rt = gamepad.right_trigger / 255.0f;
  constexpr float kTriggerDeadzone = 30.0f / 255.0f;
  io.AddKeyAnalogEvent(ImGuiKey_GamepadL2,
                       !s_modal_navigation_capture && lt > kTriggerDeadzone,
                       !s_modal_navigation_capture ? lt : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadR2,
                       !s_modal_navigation_capture && rt > kTriggerDeadzone,
                       !s_modal_navigation_capture ? rt : 0.0f);

}

void RecurseFolderForGames(std::string path) {
  try {
    const std::string normalized_directory = NormalizeScannedPath(path);

    if (HasScannedDirectory(normalized_directory)) {
      return;
    }
    s_scanned_paths.push_back(normalized_directory);

    std::filesystem::path loose_xex_path;
    std::string loose_xex_name;
    bool has_loose_xex = false;

    for (auto file : std::filesystem::directory_iterator(path)) {
      if (file.is_directory() && file.path().string() != path) {
        // Don't index Xenia's own content/cache/storage trees as games. DLC,
        // title updates and caches may contain STFS/XEX-like files that would
        // otherwise appear as duplicate game entries.
        if (!IsConfiguredStorageRoot(file.path())) {
          RecurseFolderForGames(file.path().string());
        }
        continue;
      }

      if (!file.is_regular_file()) continue;

      // Don't mount/parse disc images while scanning the frontend library.
      // Xbox storage can make synchronous XISO probing stall the UI thread.
      // Listing by extension is enough here; LaunchPath validates the image
      // when the user actually starts the game.
      std::string extension = file.path().extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                     });
      if (extension == ".iso") {
        AddGameEntry(file.path(), file.path().stem().string());
        continue;
      }

      switch (xe::GetFileSignature(file.path(), false)) {
        case xe::Emulator::FileSignatureType::XEX1:
        case xe::Emulator::FileSignatureType::XEX2: {
          const bool is_default_xex =
              _stricmp(file.path().filename().string().c_str(),
                       "default.xex") == 0;
          if (!is_default_xex) {
            break;
          }

          loose_xex_path = file.path();
          if (file.path().has_parent_path()) {
            loose_xex_name = file.path().parent_path().filename().string();
          } else {
            loose_xex_name = file.path().stem().string();
          }
          has_loose_xex = true;
          break;
        }
        case xe::Emulator::FileSignatureType::CON:
        case xe::Emulator::FileSignatureType::PIRS:
        case xe::Emulator::FileSignatureType::ZAR:
        case xe::Emulator::FileSignatureType::XISO: {
          std::string filename = file.path().stem().string();

          AddGameEntry(file.path(), filename);
          break;
        }

        case xe::Emulator::FileSignatureType::LIVE: {
          std::ifstream in(file.path().string(), std::ios::binary);

          in.seekg(0x412);

          char data[32];
          for (int i = 0; i < 32; i++) {
            char c;
            in.read(&c, 2);
            std::wctomb(&data[i], static_cast<wchar_t>(c));

            if (c == 0) break;
          }

          AddGameEntry(file.path(), data);

          in.close();
        }
        default:
          continue;
      }
    }

    if (has_loose_xex) {
      AddGameEntry(loose_xex_path, loose_xex_name);
    }
  } catch (std::exception) {
    // This folder can't be opened.
  }
}

void UWP::RefreshPaths() {
  s_paths.clear();
  s_games.clear();
  s_scanned_paths.clear();

  RecurseFolderForGames(UWP::GetLocalCache());

  if (!cvars::gamepaths.empty()) {
    std::stringstream ss (cvars::gamepaths);
    std::string item;
    while (std::getline(ss, item, ';')) {
      if (item.empty()) continue;

      const std::string normalized_item = NormalizeScannedPath(item);
      const bool duplicate = std::any_of(
          s_paths.cbegin(), s_paths.cend(), [&](const std::string& existing) {
            return NormalizeScannedPath(existing) == normalized_item;
          });
      if (duplicate) {
        continue;
      }

      s_paths.push_back(item);
      RecurseFolderForGames(item);
    }
  }
  std::stringstream deduped_paths_stream;
  for (const auto& path : s_paths) {
    deduped_paths_stream << path << ";";
  }
  const std::string deduped_paths = deduped_paths_stream.str();
  if (deduped_paths != cvars::gamepaths) {
    auto gamepaths_it = cvar::ConfigVars->find("gamepaths");
    if (gamepaths_it != cvar::ConfigVars->end()) {
      auto* gamepaths_config =
          dynamic_cast<cvar::ConfigVar<std::string>*>(gamepaths_it->second);
      if (gamepaths_config) {
        XELOGI("[UWP] Cleaning duplicate game paths: '{}' -> '{}'",
               cvars::gamepaths, deduped_paths);
        gamepaths_config->SetConfigValue(deduped_paths);
        config::SaveConfig();
      }
    }
  }

  std::sort(s_games.begin(), s_games.end(), [](auto& first, auto& second) {
    return std::get<1>(first) < std::get<1>(second);
  });
}

std::vector<std::tuple<std::string, std::string>> UWP::GetGames() {
  return s_games;
}

void UWP::SetGamePaths(std::vector<std::string> paths) {
  s_paths.clear();
  for (const auto& path : paths) {
    if (path.empty()) {
      continue;
    }
    const std::string normalized_path = NormalizeScannedPath(path);
    const bool duplicate = std::any_of(
        s_paths.cbegin(), s_paths.cend(), [&](const std::string& existing) {
          return NormalizeScannedPath(existing) == normalized_path;
        });
    if (!duplicate) {
      s_paths.push_back(path);
    }
  }

  std::stringstream ss;
  for (const auto& path : s_paths) {
    ss << path << ";";
  }

  auto cpaths = dynamic_cast<cvar::ConfigVar<std::string>*>(
      cvar::ConfigVars->find("gamepaths")->second);
  cpaths->SetConfigValue(ss.str());
  config::SaveConfig();
  RefreshPaths();
}

std::vector<std::string> UWP::GetPaths() { 
  return s_paths;
}
