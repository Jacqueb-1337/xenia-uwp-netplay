/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/xinput/xinput_input_driver.h"

// Must be included before xinput.h to avoid windows.h conflicts:
#include "xenia/base/platform_win.h"

#include <xinput.h>  // NOLINT(build/include_order)

#include <atomic>

#include "xenia/base/clock.h"
#include "xenia/base/logging.h"
#include "xenia/hid/hid_flags.h"

#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/window_uwp.h"
#endif

namespace xe {
namespace hid {
namespace xinput {

namespace {
#if XE_PLATFORM_WINRT
struct UwpSyntheticGamepadState {
  std::atomic_bool enabled{false};
  std::atomic<uint32_t> packet_number{1};
  std::atomic<uint16_t> buttons{0};
  std::atomic_bool left_trigger{false};
  std::atomic_bool right_trigger{false};
  std::atomic_bool left_up{false};
  std::atomic_bool left_down{false};
  std::atomic_bool left_left{false};
  std::atomic_bool left_right{false};
  std::atomic_bool right_up{false};
  std::atomic_bool right_down{false};
  std::atomic_bool right_left{false};
  std::atomic_bool right_right{false};
};

UwpSyntheticGamepadState g_uwp_synthetic_gamepad;

bool IsUwpSyntheticGamepadEnabled(uint32_t user_index) {
  return user_index == 0 &&
         g_uwp_synthetic_gamepad.enabled.load(std::memory_order_relaxed);
}

void ApplyUwpSyntheticGamepadState(X_INPUT_STATE* state) {
  state->packet_number +=
      g_uwp_synthetic_gamepad.packet_number.load(std::memory_order_relaxed);
  state->gamepad.buttons |=
      g_uwp_synthetic_gamepad.buttons.load(std::memory_order_relaxed);
  if (g_uwp_synthetic_gamepad.left_trigger.load(std::memory_order_relaxed)) {
    state->gamepad.left_trigger = 0xFF;
  }
  if (g_uwp_synthetic_gamepad.right_trigger.load(std::memory_order_relaxed)) {
    state->gamepad.right_trigger = 0xFF;
  }

  const bool left_up =
      g_uwp_synthetic_gamepad.left_up.load(std::memory_order_relaxed);
  const bool left_down =
      g_uwp_synthetic_gamepad.left_down.load(std::memory_order_relaxed);
  const bool left_left =
      g_uwp_synthetic_gamepad.left_left.load(std::memory_order_relaxed);
  const bool left_right =
      g_uwp_synthetic_gamepad.left_right.load(std::memory_order_relaxed);
  const bool right_up =
      g_uwp_synthetic_gamepad.right_up.load(std::memory_order_relaxed);
  const bool right_down =
      g_uwp_synthetic_gamepad.right_down.load(std::memory_order_relaxed);
  const bool right_left =
      g_uwp_synthetic_gamepad.right_left.load(std::memory_order_relaxed);
  const bool right_right =
      g_uwp_synthetic_gamepad.right_right.load(std::memory_order_relaxed);

  if (left_up != left_down) {
    state->gamepad.thumb_ly = left_up ? INT16_MAX : -INT16_MAX;
  }
  if (left_left != left_right) {
    state->gamepad.thumb_lx = left_right ? INT16_MAX : -INT16_MAX;
  }
  if (right_up != right_down) {
    state->gamepad.thumb_ry = right_up ? INT16_MAX : -INT16_MAX;
  }
  if (right_left != right_right) {
    state->gamepad.thumb_rx = right_right ? INT16_MAX : -INT16_MAX;
  }
}
#endif
}  // namespace

bool SetUwpSyntheticGamepadVirtualKey(uint32_t virtual_key, bool down) {
#if XE_PLATFORM_WINRT
  auto touch_packet = []() {
    g_uwp_synthetic_gamepad.packet_number.fetch_add(1,
                                                     std::memory_order_relaxed);
  };
  auto set_bool = [&](std::atomic_bool& value) {
    if (value.exchange(down, std::memory_order_relaxed) != down) {
      touch_packet();
    }
  };
  auto set_button = [&](uint16_t mask) {
    uint16_t old_value =
        g_uwp_synthetic_gamepad.buttons.load(std::memory_order_relaxed);
    while (true) {
      const uint16_t new_value =
          down ? static_cast<uint16_t>(old_value | mask)
               : static_cast<uint16_t>(old_value & ~mask);
      if (new_value == old_value) {
        return;
      }
      if (g_uwp_synthetic_gamepad.buttons.compare_exchange_weak(
              old_value, new_value, std::memory_order_relaxed)) {
        touch_packet();
        return;
      }
    }
  };

  bool recognized = true;
  switch (virtual_key) {
    case 0xC3:  // VK_GAMEPAD_A
      set_button(XINPUT_GAMEPAD_A);
      break;
    case 0xC4:  // VK_GAMEPAD_B
      set_button(XINPUT_GAMEPAD_B);
      break;
    case 0xC5:  // VK_GAMEPAD_X
      set_button(XINPUT_GAMEPAD_X);
      break;
    case 0xC6:  // VK_GAMEPAD_Y
      set_button(XINPUT_GAMEPAD_Y);
      break;
    case 0xC7:  // VK_GAMEPAD_RIGHT_SHOULDER
      set_button(XINPUT_GAMEPAD_RIGHT_SHOULDER);
      break;
    case 0xC8:  // VK_GAMEPAD_LEFT_SHOULDER
      set_button(XINPUT_GAMEPAD_LEFT_SHOULDER);
      break;
    case 0xC9:  // VK_GAMEPAD_LEFT_TRIGGER
      set_bool(g_uwp_synthetic_gamepad.left_trigger);
      break;
    case 0xCA:  // VK_GAMEPAD_RIGHT_TRIGGER
      set_bool(g_uwp_synthetic_gamepad.right_trigger);
      break;
    case 0xCB:  // VK_GAMEPAD_DPAD_UP
      set_button(XINPUT_GAMEPAD_DPAD_UP);
      break;
    case 0xCC:  // VK_GAMEPAD_DPAD_DOWN
      set_button(XINPUT_GAMEPAD_DPAD_DOWN);
      break;
    case 0xCD:  // VK_GAMEPAD_DPAD_LEFT
      set_button(XINPUT_GAMEPAD_DPAD_LEFT);
      break;
    case 0xCE:  // VK_GAMEPAD_DPAD_RIGHT
      set_button(XINPUT_GAMEPAD_DPAD_RIGHT);
      break;
    case 0xCF:  // VK_GAMEPAD_MENU
      set_button(XINPUT_GAMEPAD_START);
      break;
    case 0xD0:  // VK_GAMEPAD_VIEW
      set_button(XINPUT_GAMEPAD_BACK);
      break;
    case 0xD1:  // VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON
      set_button(XINPUT_GAMEPAD_LEFT_THUMB);
      break;
    case 0xD2:  // VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON
      set_button(XINPUT_GAMEPAD_RIGHT_THUMB);
      break;
    case 0xD3:  // VK_GAMEPAD_LEFT_THUMBSTICK_UP
      set_bool(g_uwp_synthetic_gamepad.left_up);
      break;
    case 0xD4:  // VK_GAMEPAD_LEFT_THUMBSTICK_DOWN
      set_bool(g_uwp_synthetic_gamepad.left_down);
      break;
    case 0xD5:  // VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT
      set_bool(g_uwp_synthetic_gamepad.left_right);
      break;
    case 0xD6:  // VK_GAMEPAD_LEFT_THUMBSTICK_LEFT
      set_bool(g_uwp_synthetic_gamepad.left_left);
      break;
    case 0xD7:  // VK_GAMEPAD_RIGHT_THUMBSTICK_UP
      set_bool(g_uwp_synthetic_gamepad.right_up);
      break;
    case 0xD8:  // VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN
      set_bool(g_uwp_synthetic_gamepad.right_down);
      break;
    case 0xD9:  // VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT
      set_bool(g_uwp_synthetic_gamepad.right_right);
      break;
    case 0xDA:  // VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT
      set_bool(g_uwp_synthetic_gamepad.right_left);
      break;
    default:
      recognized = false;
      break;
  }
  if (recognized) {
    g_uwp_synthetic_gamepad.enabled.store(true, std::memory_order_relaxed);
  }
  return recognized;
#else
  (void)virtual_key;
  (void)down;
  return false;
#endif
}

XInputInputDriver::XInputInputDriver(xe::ui::Window* window,
                                     size_t window_z_order)
    : InputDriver(window, window_z_order),
      module_(nullptr),
      XInputGetCapabilities_(nullptr),
      XInputGetState_(nullptr),
      XInputGetStateEx_(nullptr),
      XInputGetKeystroke_(nullptr),
      XInputSetState_(nullptr),
      XInputEnable_(nullptr) {}

XInputInputDriver::~XInputInputDriver() {
  if (module_) {
    FreeLibrary((HMODULE)module_);
    module_ = nullptr;
    XInputGetCapabilities_ = nullptr;
    XInputGetState_ = nullptr;
    XInputGetStateEx_ = nullptr;
    XInputGetKeystroke_ = nullptr;
    XInputSetState_ = nullptr;
    XInputEnable_ = nullptr;
  }

#if XE_PLATFORM_WINRT
  static_cast<xe::ui::UWPWindow*>(window())->ClearXInputDriver();
#endif
}

X_STATUS XInputInputDriver::Setup() {
  HMODULE module = LoadLibraryW(L"xinput1_4.dll");
  if (!module) {
    return X_STATUS_DLL_NOT_FOUND;
  }

  // Support guide button with XInput using XInputGetStateEx
  // https://source.winehq.org/git/wine.git/?a=commit;h=de3591ca9803add117fbacb8abe9b335e2e44977
  auto const XInputGetStateEx = (LPCSTR)100;

  // Required.
  auto xigc = GetProcAddress(module, "XInputGetCapabilities");
  auto xigs = GetProcAddress(module, "XInputGetState");
  auto xigsEx = GetProcAddress(module, XInputGetStateEx);
  auto xigk = GetProcAddress(module, "XInputGetKeystroke");
  auto xiss = GetProcAddress(module, "XInputSetState");

  // Not required.
  auto xie = GetProcAddress(module, "XInputEnable");

  // Only fail when we don't have the bare essentials;
  if (!xigc || !xigs || !xigk || !xiss) {
    FreeLibrary(module);
    return X_STATUS_PROCEDURE_NOT_FOUND;
  }

  module_ = module;
  XInputGetCapabilities_ = xigc;
  XInputGetState_ = xigs;
  XInputGetStateEx_ = xigsEx;
  XInputGetKeystroke_ = xigk;
  XInputSetState_ = xiss;
  XInputEnable_ = xie;

#if XE_PLATFORM_WINRT
  static_cast<xe::ui::UWPWindow*>(window())->SetXInputDriver(this);
#endif

  return X_STATUS_SUCCESS;
}

constexpr uint64_t SKIP_INVALID_CONTROLLER_TIME = 1100;
static uint64_t last_invalid_time[4];

static DWORD should_skip(uint32_t user_index) {
  uint64_t time = last_invalid_time[user_index];
  if (time) {
    uint64_t deltatime = xe::Clock::QueryHostUptimeMillis() - time;

    if (deltatime < SKIP_INVALID_CONTROLLER_TIME) {
      return ERROR_DEVICE_NOT_CONNECTED;
    }
    last_invalid_time[user_index] = 0;
  }
  return 0;
}

static void set_skip(uint32_t user_index) {
  last_invalid_time[user_index] = xe::Clock::QueryHostUptimeMillis();
}

X_RESULT XInputInputDriver::GetCapabilities(uint32_t user_index, uint32_t flags,
                                            X_INPUT_CAPABILITIES* out_caps) {
#if XE_PLATFORM_WINRT
  const bool synthetic = IsUwpSyntheticGamepadEnabled(user_index);
#else
  const bool synthetic = false;
#endif
  DWORD skipper = synthetic ? 0 : should_skip(user_index);
  if (skipper) {
    return skipper;
  }
  XINPUT_CAPABILITIES native_caps = {};
  auto xigc = (decltype(&XInputGetCapabilities))XInputGetCapabilities_;
  DWORD result =
      xigc(user_index, flags & ~X_INPUT_DEVTYPE::XINPUT_DEVTYPE_KEYBOARD,
           &native_caps);
  if (result) {
#if XE_PLATFORM_WINRT
    if (synthetic) {
      native_caps.Type = XINPUT_DEVTYPE_GAMEPAD;
      native_caps.SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
      native_caps.Gamepad.wButtons = 0xF3FF;
      native_caps.Gamepad.bLeftTrigger = 0xFF;
      native_caps.Gamepad.bRightTrigger = 0xFF;
      native_caps.Gamepad.sThumbLX = INT16_MAX;
      native_caps.Gamepad.sThumbLY = INT16_MAX;
      native_caps.Gamepad.sThumbRX = INT16_MAX;
      native_caps.Gamepad.sThumbRY = INT16_MAX;
      result = ERROR_SUCCESS;
    } else
#endif
    {
      if (result == ERROR_DEVICE_NOT_CONNECTED) {
        set_skip(user_index);
      }
      return result;
    }
  }

  out_caps->type = native_caps.Type;
  out_caps->sub_type = native_caps.SubType;
  out_caps->flags = native_caps.Flags;
  out_caps->gamepad.buttons = native_caps.Gamepad.wButtons;
  out_caps->gamepad.left_trigger = native_caps.Gamepad.bLeftTrigger;
  out_caps->gamepad.right_trigger = native_caps.Gamepad.bRightTrigger;
  out_caps->gamepad.thumb_lx = native_caps.Gamepad.sThumbLX;
  out_caps->gamepad.thumb_ly = native_caps.Gamepad.sThumbLY;
  out_caps->gamepad.thumb_rx = native_caps.Gamepad.sThumbRX;
  out_caps->gamepad.thumb_ry = native_caps.Gamepad.sThumbRY;
  out_caps->vibration.left_motor_speed = native_caps.Vibration.wLeftMotorSpeed;
  out_caps->vibration.right_motor_speed =
      native_caps.Vibration.wRightMotorSpeed;

  return result;
}

X_RESULT XInputInputDriver::GetState(uint32_t user_index,
                                     X_INPUT_STATE* out_state) {
#if XE_PLATFORM_WINRT
  const bool synthetic = IsUwpSyntheticGamepadEnabled(user_index);
#else
  const bool synthetic = false;
#endif
  DWORD skipper = synthetic ? 0 : should_skip(user_index);
  if (skipper) {
    return skipper;
  }

  // Added padding in case we are using XInputGetStateEx.
  struct {
    XINPUT_STATE state;
    unsigned int dwPaddingReserved;
  } native_state = {};

  auto xigs = (decltype(&XInputGetState))XInputGetState_;
  DWORD result = ERROR_PROC_NOT_FOUND;
#if XE_PLATFORM_WINRT
  // Xbox UWP does not reliably support XInputGetStateEx. Always use the
  // public XInputGetState path here. The Device Portal synthetic controller
  // is merged below and can also provide user 0 when no physical pad exists.
  result = xigs(user_index, &native_state.state);
  if (result && synthetic) {
    native_state.state = {};
    result = ERROR_SUCCESS;
  }
#else
  if (cvars::guide_button && XInputGetStateEx_) {
    auto xigs_ex = (decltype(&XInputGetState))XInputGetStateEx_;
    result = xigs_ex(user_index, &native_state.state);
  }
  if (result == ERROR_PROC_NOT_FOUND) {
    result = xigs(user_index, &native_state.state);
  }
#endif
  if (result) {
    if (result == ERROR_DEVICE_NOT_CONNECTED) {
      set_skip(user_index);
    }
    return result;
  }

  out_state->packet_number = native_state.state.dwPacketNumber;
  out_state->gamepad.buttons = native_state.state.Gamepad.wButtons;
  out_state->gamepad.left_trigger = native_state.state.Gamepad.bLeftTrigger;
  out_state->gamepad.right_trigger = native_state.state.Gamepad.bRightTrigger;
  out_state->gamepad.thumb_lx = native_state.state.Gamepad.sThumbLX;
  out_state->gamepad.thumb_ly = native_state.state.Gamepad.sThumbLY;
  out_state->gamepad.thumb_rx = native_state.state.Gamepad.sThumbRX;
  out_state->gamepad.thumb_ry = native_state.state.Gamepad.sThumbRY;

#if XE_PLATFORM_WINRT
  if (synthetic) {
    ApplyUwpSyntheticGamepadState(out_state);
  }

  // Xbox UWP can't receive the physical Xbox button through normal XInput.
  // Treat Start + View/Back as the Xbox 360 Guide button instead. This also
  // works for Device Portal's synthetic Menu + View presses.
  constexpr uint16_t kGuideCombo = XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK;
  if ((out_state->gamepad.buttons & kGuideCombo) == kGuideCombo) {
    out_state->gamepad.buttons = static_cast<uint16_t>(
        (out_state->gamepad.buttons & ~kGuideCombo) | X_INPUT_GAMEPAD_GUIDE);
  }
#endif

  return result;
}

X_RESULT XInputInputDriver::SetState(uint32_t user_index,
                                     X_INPUT_VIBRATION* vibration) {
  DWORD skipper = should_skip(user_index);
  if (skipper) {
    return skipper;
  }
  XINPUT_VIBRATION native_vibration;
  native_vibration.wLeftMotorSpeed = vibration->left_motor_speed;
  native_vibration.wRightMotorSpeed = vibration->right_motor_speed;
  auto xiss = (decltype(&XInputSetState))XInputSetState_;
  DWORD result = xiss(user_index, &native_vibration);
  if (result == ERROR_DEVICE_NOT_CONNECTED) {
    set_skip(user_index);
  }
  return result;
}

X_RESULT XInputInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                         X_INPUT_KEYSTROKE* out_keystroke) {
  // We may want to filter flags/user_index before sending to native.
  // flags is reserved on desktop.
  DWORD result;

  // XInputGetKeystroke on Windows has a bug where it will return
  // ERROR_SUCCESS (0) even if the device is not connected:
  // https://stackoverflow.com/questions/23669238/xinputgetkeystroke-returning-error-success-while-controller-is-unplugged
  //
  // So we first check if the device is connected via XInputGetCapabilities, so
  // we are not passing back an uninitialized X_INPUT_KEYSTROKE structure.
  // If any user (0xFF) is polled this bug does not occur but GetCapabilities
  // would fail so we need to skip it.
  if (user_index != XUserIndexAny) {
    XINPUT_CAPABILITIES caps;
    auto xigc = (decltype(&XInputGetCapabilities))XInputGetCapabilities_;
    result = xigc(user_index, 0, &caps);
    if (result) {
      return result;
    }
  }

  XINPUT_KEYSTROKE native_keystroke;
  auto xigk = (decltype(&XInputGetKeystroke))XInputGetKeystroke_;
  result = xigk(user_index, 0, &native_keystroke);
  if (result) {
    return result;
  }

  out_keystroke->virtual_key = native_keystroke.VirtualKey;
  out_keystroke->unicode = native_keystroke.Unicode;
  out_keystroke->flags = native_keystroke.Flags;
  out_keystroke->user_index = native_keystroke.UserIndex;
  out_keystroke->hid_code = native_keystroke.HidCode;
  // X_ERROR_EMPTY if no new keys
  // X_ERROR_DEVICE_NOT_CONNECTED if no device
  // X_ERROR_SUCCESS if key
  return result;
}

InputType XInputInputDriver::GetInputType() const {
  return InputType::Controller;
}

}  // namespace xinput
}  // namespace hid
}  // namespace xe
