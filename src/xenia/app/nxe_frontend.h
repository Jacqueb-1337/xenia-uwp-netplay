/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * NXE-inspired frontend chrome for the WinRT/UWP shell.                      *
 * This is an original implementation using Dear ImGui primitives.            *
 ******************************************************************************
 */
#ifndef XENIA_APP_NXE_FRONTEND_H_
#define XENIA_APP_NXE_FRONTEND_H_

#include <algorithm>
#include <array>

#include "third_party/imgui/imgui.h"

namespace xe::app::nxe {

struct Palette {
  static constexpr ImU32 kGreen = IM_COL32(110, 170, 36, 255);
  static constexpr ImU32 kGreenBright = IM_COL32(145, 205, 54, 255);
  static constexpr ImU32 kGreenDark = IM_COL32(55, 96, 20, 255);
  static constexpr ImU32 kText = IM_COL32(248, 248, 248, 255);
  static constexpr ImU32 kTextMuted = IM_COL32(205, 211, 208, 255);
  static constexpr ImU32 kChromeDark = IM_COL32(31, 36, 34, 238);
  static constexpr ImU32 kChromeMid = IM_COL32(59, 66, 63, 226);
  static constexpr ImU32 kPanel = IM_COL32(18, 22, 20, 208);
  static constexpr ImU32 kPanelLight = IM_COL32(255, 255, 255, 31);
};

inline void DrawBackdrop(ImDrawList* draw_list, const ImVec2& display_size) {
  if (!draw_list || display_size.x <= 0.0f || display_size.y <= 0.0f) {
    return;
  }

  const ImVec2 min(0.0f, 0.0f);
  const ImVec2 horizon(display_size.x, display_size.y * 0.69f);
  const ImVec2 max(display_size.x, display_size.y);

  draw_list->AddRectFilledMultiColor(
      min, horizon, IM_COL32(54, 63, 60, 255), IM_COL32(28, 34, 32, 255),
      IM_COL32(151, 159, 154, 255), IM_COL32(167, 173, 169, 255));
  draw_list->AddRectFilledMultiColor(
      ImVec2(0.0f, horizon.y), max, IM_COL32(113, 120, 116, 255),
      IM_COL32(101, 108, 104, 255), IM_COL32(35, 40, 38, 255),
      IM_COL32(31, 36, 34, 255));

  // NXE's stage had a glossy horizon and a floor that visually receded away
  // from the selected content. Subtle perspective guides reproduce that depth
  // without depending on dashboard assets.
  const float cx = display_size.x * 0.5f;
  const float floor_top = horizon.y;
  const float floor_bottom = display_size.y;
  const ImU32 guide = IM_COL32(255, 255, 255, 18);
  for (int i = -6; i <= 6; ++i) {
    const float bottom_x = cx + static_cast<float>(i) * display_size.x * 0.12f;
    const float top_x = cx + static_cast<float>(i) * display_size.x * 0.016f;
    draw_list->AddLine(ImVec2(top_x, floor_top),
                       ImVec2(bottom_x, floor_bottom), guide, 1.0f);
  }
  for (int i = 1; i <= 5; ++i) {
    const float t = static_cast<float>(i) / 6.0f;
    const float y = floor_top + (floor_bottom - floor_top) * t * t;
    draw_list->AddLine(ImVec2(0.0f, y), ImVec2(display_size.x, y), guide,
                       1.0f);
  }

  draw_list->AddRectFilledMultiColor(
      ImVec2(0.0f, floor_top - 10.0f), ImVec2(display_size.x, floor_top + 18.0f),
      IM_COL32(255, 255, 255, 3), IM_COL32(255, 255, 255, 3),
      IM_COL32(255, 255, 255, 38), IM_COL32(255, 255, 255, 38));
}

inline void DrawTopChrome(ImDrawList* draw_list, const ImVec2& display_size,
                          float scale) {
  const float h = std::max(54.0f, 68.0f * scale);
  draw_list->AddRectFilledMultiColor(
      ImVec2(0.0f, 0.0f), ImVec2(display_size.x, h),
      IM_COL32(22, 27, 25, 248), IM_COL32(33, 39, 36, 248),
      IM_COL32(57, 64, 61, 234), IM_COL32(46, 52, 49, 234));
  draw_list->AddLine(ImVec2(0.0f, h), ImVec2(display_size.x, h),
                     IM_COL32(255, 255, 255, 38),
                     std::max(1.0f, 1.5f * scale));
}

inline void DrawBottomChrome(ImDrawList* draw_list, const ImVec2& display_size,
                             float scale) {
  const float h = std::max(42.0f, 52.0f * scale);
  const float y = display_size.y - h;
  draw_list->AddRectFilledMultiColor(
      ImVec2(0.0f, y), display_size, IM_COL32(44, 49, 47, 226),
      IM_COL32(28, 33, 31, 226), IM_COL32(18, 21, 20, 246),
      IM_COL32(18, 21, 20, 246));
  draw_list->AddLine(ImVec2(0.0f, y), ImVec2(display_size.x, y),
                     IM_COL32(255, 255, 255, 28), 1.0f);
}

inline void DrawGlassPanel(ImDrawList* draw_list, const ImVec2& min,
                           const ImVec2& max, float rounding, bool selected) {
  if (selected) {
    draw_list->AddRectFilled(ImVec2(min.x + 8.0f, min.y + 10.0f),
                             ImVec2(max.x + 8.0f, max.y + 10.0f),
                             IM_COL32(0, 0, 0, 75), rounding);
  }
  draw_list->AddRectFilledMultiColor(
      min, max,
      selected ? IM_COL32(118, 178, 39, 243) : IM_COL32(36, 42, 39, 222),
      selected ? IM_COL32(80, 139, 26, 243) : IM_COL32(27, 32, 30, 222),
      selected ? IM_COL32(43, 78, 17, 248) : IM_COL32(15, 19, 17, 238),
      selected ? IM_COL32(59, 105, 19, 248) : IM_COL32(20, 24, 22, 238));
  draw_list->AddRect(min, max,
                     selected ? IM_COL32(218, 245, 177, 216)
                              : IM_COL32(255, 255, 255, 34),
                     rounding, 0, selected ? 2.0f : 1.0f);
  draw_list->AddLine(ImVec2(min.x + 3.0f, min.y + 3.0f),
                     ImVec2(max.x - 3.0f, min.y + 3.0f),
                     selected ? IM_COL32(255, 255, 255, 105)
                              : IM_COL32(255, 255, 255, 25),
                     1.0f);
}

inline void PushSettingsContentStyle(float scale) {
  scale = std::max(0.75f, scale);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(14.0f * scale, 10.0f * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(10.0f * scale, 9.0f * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f * scale);

  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(26, 31, 29, 235));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(61, 88, 37, 245));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(78, 122, 34, 250));
  ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(34, 40, 37, 238));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(84, 132, 34, 248));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(103, 159, 39, 255));
  ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(44, 52, 48, 240));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(81, 128, 34, 248));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(104, 161, 40, 255));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, Palette::kGreen);
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Palette::kGreenBright);
  ImGui::PushStyleColor(ImGuiCol_CheckMark, Palette::kText);
  ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(21, 25, 23, 252));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 48));
  ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, IM_COL32(104, 161, 40, 150));
}

inline void PopSettingsContentStyle() {
  ImGui::PopStyleColor(15);
  ImGui::PopStyleVar(4);
}

// NXE settings don't present boolean values as tiny desktop checkboxes. They
// are full-width focusable rows, with the value separated visually on the
// right. Using Selectable preserves ImGui's controller navigation semantics:
// D-pad moves focus and A activates the row.
inline bool ToggleRow(const char* label, bool* value) {
  if (!label || !value) {
    return false;
  }

  const float scale = std::max(0.75f, ImGui::GetFontSize() / 18.0f);
  const float row_height = 50.0f * scale;
  const float rounding = 4.0f * scale;
  const float width = std::max(180.0f * scale, ImGui::GetContentRegionAvail().x);

  ImGui::PushID(label);
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
  const bool activated = ImGui::Selectable(
      "##nxe_toggle", false, ImGuiSelectableFlags_None,
      ImVec2(width, row_height));
  ImGui::PopStyleColor(4);

  if (activated) {
    *value = !*value;
  }

  const bool focused = ImGui::IsItemFocused();
  const bool highlighted = focused || ImGui::IsItemHovered() ||
                           ImGui::IsItemActive();
  const ImVec2 row_min = ImGui::GetItemRectMin();
  const ImVec2 row_max = ImGui::GetItemRectMax();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  DrawGlassPanel(draw_list, row_min, row_max, rounding, highlighted);

  const float font_size = ImGui::GetFontSize();
  const ImVec2 label_size = ImGui::CalcTextSize(label);
  const ImVec2 label_pos(row_min.x + 15.0f * scale,
                         row_min.y + (row_height - label_size.y) * 0.5f);
  draw_list->AddText(ImGui::GetFont(), font_size, label_pos, Palette::kText,
                     label);

  const char* value_text = *value ? "ON" : "OFF";
  const ImVec2 value_size = ImGui::CalcTextSize(value_text);
  const float pill_w = 64.0f * scale;
  const float pill_h = 28.0f * scale;
  const ImVec2 pill_max(row_max.x - 12.0f * scale,
                        row_min.y + (row_height + pill_h) * 0.5f);
  const ImVec2 pill_min(pill_max.x - pill_w, pill_max.y - pill_h);
  draw_list->AddRectFilled(
      pill_min, pill_max,
      *value ? IM_COL32(116, 178, 39, 245) : IM_COL32(8, 11, 10, 125),
      pill_h * 0.48f);
  draw_list->AddRect(pill_min, pill_max,
                     *value ? IM_COL32(221, 244, 184, 180)
                            : IM_COL32(255, 255, 255, 42),
                     pill_h * 0.48f);
  draw_list->AddText(
      ImGui::GetFont(), font_size * 0.82f,
      ImVec2(pill_min.x + (pill_w - value_size.x * 0.82f) * 0.5f,
             pill_min.y + (pill_h - font_size * 0.82f) * 0.42f),
      *value ? Palette::kText : Palette::kTextMuted, value_text);

  ImGui::PopID();
  return activated;
}

inline bool MenuRow(const char* label, bool selected = false,
                    float height_scale = 1.0f) {
  if (!label) {
    return false;
  }

  const float scale = std::max(0.75f, ImGui::GetFontSize() / 18.0f);
  const float row_height = 46.0f * scale * height_scale;
  const float width = std::max(180.0f * scale, ImGui::GetContentRegionAvail().x);

  ImGui::PushID(label);
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
  const bool activated = ImGui::Selectable(
      "##nxe_menu_row", selected, ImGuiSelectableFlags_None,
      ImVec2(width, row_height));
  ImGui::PopStyleColor(4);

  const bool highlighted = selected || ImGui::IsItemFocused() ||
                           ImGui::IsItemHovered() || ImGui::IsItemActive();
  const ImVec2 row_min = ImGui::GetItemRectMin();
  const ImVec2 row_max = ImGui::GetItemRectMax();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  DrawGlassPanel(draw_list, row_min, row_max, 4.0f * scale, highlighted);

  const ImVec2 text_size = ImGui::CalcTextSize(label);
  draw_list->AddText(
      ImGui::GetFont(), ImGui::GetFontSize(),
      ImVec2(row_min.x + 15.0f * scale,
             row_min.y + (row_height - text_size.y) * 0.5f),
      Palette::kText, label);

  // Small forward chevron - enough to communicate that A enters/activates
  // without relying on dashboard artwork.
  const float chevron_x = row_max.x - 18.0f * scale;
  const float chevron_y = (row_min.y + row_max.y) * 0.5f;
  const ImU32 chevron_color = highlighted ? Palette::kText
                                          : Palette::kTextMuted;
  draw_list->AddLine(ImVec2(chevron_x - 4.0f * scale,
                            chevron_y - 5.0f * scale),
                     ImVec2(chevron_x + 1.0f * scale, chevron_y),
                     chevron_color, std::max(1.0f, 1.5f * scale));
  draw_list->AddLine(ImVec2(chevron_x + 1.0f * scale, chevron_y),
                     ImVec2(chevron_x - 4.0f * scale,
                            chevron_y + 5.0f * scale),
                     chevron_color, std::max(1.0f, 1.5f * scale));

  ImGui::PopID();
  return activated;
}

inline void DrawSectionTitle(ImDrawList* draw_list, ImFont* font,
                             float font_size, const ImVec2& pos,
                             const char* title) {
  if (!draw_list || !font || !title) {
    return;
  }
  draw_list->AddText(font, font_size, ImVec2(pos.x + 1.0f, pos.y + 2.0f),
                     IM_COL32(0, 0, 0, 125), title);
  draw_list->AddText(font, font_size, pos, Palette::kText, title);
}

inline void DrawNavTab(ImDrawList* draw_list, ImFont* font, float font_size,
                       const ImVec2& min, const ImVec2& max, const char* text,
                       bool active) {
  if (active) {
    draw_list->AddRectFilledMultiColor(
        min, max, IM_COL32(132, 190, 47, 247), IM_COL32(95, 154, 30, 247),
        IM_COL32(55, 101, 19, 247), IM_COL32(67, 119, 20, 247));
    draw_list->AddLine(ImVec2(min.x, max.y - 1.0f),
                       ImVec2(max.x, max.y - 1.0f),
                       IM_COL32(205, 237, 153, 210), 2.0f);
  }

  const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
  const ImVec2 text_pos(
      min.x + (max.x - min.x - text_size.x) * 0.5f,
      min.y + (max.y - min.y - text_size.y) * 0.5f - 1.0f);
  draw_list->AddText(font, font_size, ImVec2(text_pos.x + 1.0f, text_pos.y + 2.0f),
                     IM_COL32(0, 0, 0, active ? 120 : 165), text);
  draw_list->AddText(font, font_size, text_pos,
                     active ? Palette::kText : Palette::kTextMuted, text);
}

inline void DrawProfilePill(ImDrawList* draw_list, ImFont* font,
                            const ImVec2& display_size, float scale,
                            const char* label) {
  const float width = 220.0f * scale;
  const float height = 42.0f * scale;
  const ImVec2 max(display_size.x - 25.0f * scale, 57.0f * scale);
  const ImVec2 min(max.x - width, max.y - height);
  draw_list->AddRectFilled(min, max, IM_COL32(7, 10, 9, 116),
                           height * 0.48f);
  draw_list->AddCircleFilled(
      ImVec2(min.x + height * 0.5f, min.y + height * 0.5f),
      height * 0.32f, Palette::kGreen);
  const float font_size = 18.0f * scale;
  draw_list->AddText(font, font_size,
                     ImVec2(min.x + height, min.y + (height - font_size) * 0.48f),
                     Palette::kText, label ? label : "Xenia Canary");
}

}  // namespace xe::app::nxe

#endif  // XENIA_APP_NXE_FRONTEND_H_
