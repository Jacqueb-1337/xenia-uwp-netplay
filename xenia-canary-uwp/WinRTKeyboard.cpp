#include "WinRTKeyboard.h"

#ifdef WINRT_LEAN_AND_MEAN
#undef WINRT_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <string_view>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Text.Core.h>
#include <winrt/Windows.UI.ViewManagement.Core.h>

#include "xenia/base/logging.h"

namespace UWP {
std::vector<uint32_t> g_char_buffer;
std::mutex g_buffer_mutex;

namespace {
using namespace winrt::Windows::UI::Text::Core;

CoreTextEditContext g_edit_context{nullptr};
std::wstring g_edit_text;
CoreTextRange g_selection{};
bool g_text_input_active = false;

CoreTextRange ClampRange(CoreTextRange range, size_t text_length) {
  const int32_t end = static_cast<int32_t>(text_length);
  range.StartCaretPosition =
      std::clamp(range.StartCaretPosition, int32_t{0}, end);
  range.EndCaretPosition =
      std::clamp(range.EndCaretPosition, range.StartCaretPosition, end);
  return range;
}

void EnsureEditContext() {
  if (g_edit_context) {
    return;
  }

  auto manager = CoreTextServicesManager::GetForCurrentView();
  g_edit_context = manager.CreateEditContext();
  g_edit_context.Name(L"Xenia Gamertag");

  g_edit_context.TextRequested(
      [](const CoreTextEditContext&,
         const CoreTextTextRequestedEventArgs& args) {
        std::scoped_lock lock(g_buffer_mutex);
        auto request = args.Request();
        const CoreTextRange range = ClampRange(request.Range(), g_edit_text.size());
        const size_t start = static_cast<size_t>(range.StartCaretPosition);
        const size_t length = static_cast<size_t>(range.EndCaretPosition -
                                                  range.StartCaretPosition);
        request.Text(winrt::hstring(
            std::wstring_view(g_edit_text).substr(start, length)));
      });

  g_edit_context.SelectionRequested(
      [](const CoreTextEditContext&,
         const CoreTextSelectionRequestedEventArgs& args) {
        std::scoped_lock lock(g_buffer_mutex);
        args.Request().Selection(ClampRange(g_selection, g_edit_text.size()));
      });

  g_edit_context.TextUpdating(
      [](const CoreTextEditContext&,
         const CoreTextTextUpdatingEventArgs& args) {
        std::scoped_lock lock(g_buffer_mutex);
        CoreTextRange range = ClampRange(args.Range(), g_edit_text.size());
        const size_t start = static_cast<size_t>(range.StartCaretPosition);
        const size_t length = static_cast<size_t>(range.EndCaretPosition -
                                                  range.StartCaretPosition);
        const std::wstring replacement(args.Text().c_str());

        g_edit_text.replace(start, length, replacement);
        // Xbox 360 gamertags are at most 15 characters. Keeping the CoreText
        // buffer bounded also prevents the OSK from outrunning gamertag_.
        if (g_edit_text.size() > 15) {
          g_edit_text.resize(15);
        }

        g_selection = ClampRange(args.NewSelection(), g_edit_text.size());
        XELOGI("UWP text input updated ({} chars)", g_edit_text.size());
      });

  g_edit_context.SelectionUpdating(
      [](const CoreTextEditContext&,
         const CoreTextSelectionUpdatingEventArgs& args) {
        std::scoped_lock lock(g_buffer_mutex);
        g_selection = ClampRange(args.Selection(), g_edit_text.size());
      });

  g_edit_context.FocusRemoved(
      [](const CoreTextEditContext&,
         const winrt::Windows::Foundation::IInspectable&) {
        std::scoped_lock lock(g_buffer_mutex);
        g_text_input_active = false;
      });
}
}  // namespace

void BeginTextInput(const std::string& initial_text) {
  EnsureEditContext();

  {
    std::scoped_lock lock(g_buffer_mutex);
    const winrt::hstring initial = winrt::to_hstring(initial_text);
    g_edit_text.assign(initial.c_str());
    if (g_edit_text.size() > 15) {
      g_edit_text.resize(15);
    }
    const int32_t caret = static_cast<int32_t>(g_edit_text.size());
    g_selection.StartCaretPosition = caret;
    g_selection.EndCaretPosition = caret;
    g_text_input_active = true;
  }

  XELOGI("UWP text input: focus entered");
  g_edit_context.NotifyFocusEnter();
  winrt::Windows::UI::ViewManagement::Core::CoreInputView::GetForCurrentView()
      .TryShowPrimaryView();
}

void EndTextInput() {
  if (!g_edit_context) {
    return;
  }

  bool was_active = false;
  {
    std::scoped_lock lock(g_buffer_mutex);
    was_active = g_text_input_active;
    g_text_input_active = false;
  }
  if (!was_active) {
    return;
  }

  XELOGI("UWP text input: focus left");
  g_edit_context.NotifyFocusLeave();
  winrt::Windows::UI::ViewManagement::Core::CoreInputView::GetForCurrentView()
      .TryHidePrimaryView();
}

std::string GetTextInput() {
  std::scoped_lock lock(g_buffer_mutex);
  return winrt::to_string(winrt::hstring(g_edit_text));
}

bool IsTextInputActive() {
  std::scoped_lock lock(g_buffer_mutex);
  return g_text_input_active;
}

void ShowKeyboard() { BeginTextInput(GetTextInput()); }

void HandleBackspace() {
  winrt::Windows::UI::Text::Core::CoreTextRange modified_range{};
  winrt::Windows::UI::Text::Core::CoreTextRange new_selection{};
  bool changed = false;

  {
    std::scoped_lock lock(g_buffer_mutex);
    if (!g_text_input_active || g_edit_text.empty()) {
      return;
    }

    const winrt::Windows::UI::Text::Core::CoreTextRange selection =
        ClampRange(g_selection, g_edit_text.size());
    int32_t start = selection.StartCaretPosition;
    int32_t end = selection.EndCaretPosition;

    if (start == end) {
      if (start <= 0) {
        return;
      }
      --start;
    }

    modified_range.StartCaretPosition = start;
    modified_range.EndCaretPosition = end;
    g_edit_text.erase(static_cast<size_t>(start),
                      static_cast<size_t>(end - start));

    g_selection.StartCaretPosition = start;
    g_selection.EndCaretPosition = start;
    new_selection = g_selection;
    changed = true;
  }

  if (changed && g_edit_context) {
    // CoreText doesn't consistently turn the Xbox OSK's backspace key into a
    // TextUpdating event for custom non-XAML controls, so apply it ourselves
    // and tell the text service what changed.
    g_edit_context.NotifyTextChanged(modified_range, 0, new_selection);
    XELOGI("UWP text input backspace");
  }
}

void HandleCharacter(uint32_t keycode) {
  std::unique_lock lock(g_buffer_mutex);
  // When CoreText owns focus it is the authoritative input path. Feeding the
  // same character through CharacterReceived as well can duplicate OSK input.
  if (g_text_input_active) {
    return;
  }
  g_char_buffer.push_back(keycode);
}
}  // namespace UWP
