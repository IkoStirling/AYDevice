#include "AYTextInput.h"

namespace ayt::device {

void TextInput::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (!enabled) {
        _composing = false;
        _composition.clear();
        _compositionCursor = 0;
    }
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
    _composing = true;
    _composition.assign(utf8 != nullptr && byteCount > 0
                            ? std::string(utf8, static_cast<size_t>(byteCount))
                            : std::string());
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
