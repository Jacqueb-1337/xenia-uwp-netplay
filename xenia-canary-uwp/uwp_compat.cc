/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * UWP compatibility shims for desktop-only dependencies pulled in by the     *
 * shared native libraries.                                                   *
 ******************************************************************************
 */

#include <span>

#include "xenia/base/cvar.h"
#include "xenia/app/discord/discord_presence.h"
#include "xenia/hid/portal/hardware_portal.h"

// Discord RPC is intentionally unavailable in the Xbox UWP build, but current
// netplay's user tracker references the shared setting and update hook.
DEFINE_bool(discord, false, "Enable Discord rich presence", "General");

namespace xe::discord {

void DiscordPresence::Update() {}

}  // namespace xe::discord

namespace xe::hid {

// The native Windows HID library constructs a HardwarePortal because it is
// compiled as a desktop static library. Xbox UWP cannot use the libusb desktop
// backend, so keep the common portal API available while reporting no device.
HardwarePortal::HardwarePortal() : Portal() {}
HardwarePortal::~HardwarePortal() = default;

bool HardwarePortal::IsConnected() { return false; }

void HardwarePortal::OnDeviceArrival() {}
void HardwarePortal::OnDeviceRemoval() {}
void HardwarePortal::OpenDevice() {}
void HardwarePortal::CloseDevice() {}

X_STATUS HardwarePortal::ReadInternal(std::span<uint8_t> data,
                                      int32_t& read_count) {
  read_count = 0;
  return X_ERROR_DEVICE_NOT_CONNECTED;
}

X_STATUS HardwarePortal::WriteInternal(std::span<uint8_t> data) {
  return X_ERROR_DEVICE_NOT_CONNECTED;
}

}  // namespace xe::hid
