#pragma once
// AYDevice/TextInput.h - Committed text + IME composition (UTF-8)

#include "AYDevice/InputTypes.h"

#include <functional>
#include <string>

namespace ayt::device {

// Collects text the user typed and tracks in-progress IME composition.
//
// Two streams, both UTF-8:
//   - committed text: finished characters (WM_CHAR or IME result). Accumulated
//     per frame in getText(); also delivered via onCommit as it arrives.
//   - composition:    the candidate string the IME is still editing
//     (WM_IME_COMPOSITION). Replaced wholesale; delivered via onComposition.
//
// Enable() must be true for events to be recorded; UI focus code toggles it so
// game keybinds don't double-fire while typing.
//
// Frame flow:
//   1. newFrame()   -> clear this frame's committed text buffer
//   2. onChar/...   -> platform feeds WM_CHAR / IME events
//   3. getText()    -> this frame's committed UTF-8
class TextInput {
public:
    // ===== Enable / focus =====
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // ===== IInputDevice-style frame boundary =====
    void newFrame();

    // ===== Event feed (called by the platform layer) =====
    // Append committed UTF-8 text (already decoded from WM_CHAR / IME result).
    void onChar(const char* utf8, int byteCount);
    // Replace the current IME composition string; cursor is a byte offset.
    void onComposition(const char* utf8, int byteCount, int cursor);
    // Finish composition (committed text arrives separately via onChar).
    void endComposition();
    void reset();

    // ===== Queries =====
    // Committed text collected this frame (empty if none).
    const std::string& getText() const { return _committed; }
    bool hasText() const { return !_committed.empty(); }

    bool isComposing() const { return _composing; }
    const std::string& getComposition() const { return _composition; }
    int getCompositionCursor() const { return _compositionCursor; }

    // ===== Callbacks (fire as events arrive, before the next newFrame) =====
    std::function<void(const std::string&)> onCommit;                 // committed chunk
    std::function<void(const std::string&, int cursor)> onCompositionUpdate;

private:
    bool _enabled = false;
    bool _composing = false;

    std::string _committed;         // this frame's committed text
    std::string _composition;       // current IME candidate
    int         _compositionCursor = 0;
};

} // namespace ayt::device
