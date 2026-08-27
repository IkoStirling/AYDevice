#include "AYDevice/TextInput.h"

namespace ayt::device {

void TextInput::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }
    _enabled = enabled;
    if (!enabled) {
        _composing = false;
        _composition.clear();
        _compositionCursor = 0;
    }
    if (_onEnabledChanged) {
        _onEnabledChanged(enabled);
    }
}

void TextInput::setEnabledChangedCallback(std::function<void(bool)> callback)
{
    _onEnabledChanged = std::move(callback);
}

void TextInput::newFrame()
{
    _committed.clear();
}

void TextInput::onChar(const char* utf8, int byteCount)
{
    if (!_enabled || utf8 == nullptr || byteCount <= 0) {
        return;
    }
    const std::string chunk(utf8, static_cast<size_t>(byteCount));
    _committed += chunk;
    if (onCommit) {
        onCommit(chunk);
    }
}

void TextInput::onComposition(const char* utf8, int byteCount, int cursor)
{
    if (!_enabled) {
        return;
    }
    // L16 (2026-08-26): defensive nullptr+count guard. If a buggy
    // upstream caller passes utf8=nullptr with byteCount>0 (or
    // byteCount<=0 with utf8!=nullptr), treat as empty composition
    // rather than calling the std::string(const char*, size_t)
    // constructor with invalid arguments. Also handle negative
    // byteCount explicitly so we never underflow size_t.
    if (utf8 == nullptr || byteCount <= 0) {
        _composing = true;
        _composition.clear();
        _compositionCursor = 0;
        if (onCompositionUpdate) {
            onCompositionUpdate(_composition, _compositionCursor);
        }
        return;
    }
    _composing = true;
    _composition.assign(utf8, static_cast<size_t>(byteCount));
    _compositionCursor = cursor;
    if (onCompositionUpdate) {
        onCompositionUpdate(_composition, _compositionCursor);
    }
}

void TextInput::endComposition()
{
    _composing = false;
    _composition.clear();
    _compositionCursor = 0;
    if (onCompositionUpdate) {
        onCompositionUpdate(_composition, 0);
    }
}

void TextInput::reset()
{
    _committed.clear();
    _composition.clear();
    _compositionCursor = 0;
    _composing = false;
}

} // namespace ayt::device
