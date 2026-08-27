#include "AYDevice/TouchDevice.h"

#include <algorithm>
#include <cstdio>

namespace ayt::device {

namespace {
// L15 (2026-08-26): touch diagnostic counters. Each per-frame drop is
// logged once at first occurrence and every 256 thereafter to avoid
// stderr spam. Enabled by setting AY_DEVICE_TRACE_INPUT=1 (same env
// flag as WindowManager PR-InputTrace).
bool ayDeviceTouchTraceEnabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* env = std::getenv("AY_DEVICE_TRACE_INPUT");
        cached = (env != nullptr && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }
    return cached != 0;
}
int s_orphanDropCount = 0;
} // namespace

TouchDevice::TouchDevice() = default;

TouchPoint* TouchDevice::findById(int64_t id)
{
    for (TouchPoint& point : _points) {
        if (point.id == id) {
            return &point;
        }
    }
    return nullptr;
}

void TouchDevice::newFrame()
{
    // Retire contacts that ended last frame; survivors default to Stationary
    // with a cleared delta until this frame's events arrive.
    _points.erase(
        std::remove_if(_points.begin(), _points.end(),
                       [](const TouchPoint& p) {
                           return p.phase == TouchPhase::Ended
                               || p.phase == TouchPhase::Cancelled;
                       }),
        _points.end());

    for (TouchPoint& point : _points) {
        point.phase = TouchPhase::Stationary;
        point.delta = Vector2{};
    }
}

void TouchDevice::onTouch(int64_t id, float x, float y, TouchPhase phase)
{
    const Vector2 position{x, y};

    if (phase == TouchPhase::Began) {
        // Replace any stale contact reusing this id, else add a new one.
        if (TouchPoint* existing = findById(id)) {
            existing->position = position;
            existing->delta = Vector2{};
            existing->pressure = 1.0f;
            existing->phase = TouchPhase::Began;
            return;
        }
        TouchPoint point{};
        point.id = id;
        point.position = position;
        point.delta = Vector2{};
        point.pressure = 1.0f;
        point.phase = TouchPhase::Began;
        _points.push_back(point);
        return;
    }

    TouchPoint* point = findById(id);
    if (point == nullptr) {
        // Move/End for an unknown id: treat as a Began so it is not lost.
        if (phase == TouchPhase::Ended || phase == TouchPhase::Cancelled) {
            // L15 (2026-08-26): diagnostic counter. Track the orphan
            // End/Cancelled so driver drops are visible in traces.
            if (ayDeviceTouchTraceEnabled()) {
                ++s_orphanDropCount;
                if (s_orphanDropCount == 1 || (s_orphanDropCount % 256) == 0) {
                    std::fprintf(stderr,
                        "[AYDevice-InputTrace] TouchDevice orphan drop #%d (id=%lld, phase=%d)\n",
                        s_orphanDropCount,
                        static_cast<long long>(id),
                        static_cast<int>(phase));
                }
            }
            return;
        }
        TouchPoint added{};
        added.id = id;
        added.position = position;
        added.pressure = 1.0f;
        added.phase = TouchPhase::Began;
        _points.push_back(added);
        return;
    }

    point->delta.x += position.x - point->position.x;
    point->delta.y += position.y - point->position.y;
    point->position = position;
    point->phase = phase;
}

void TouchDevice::cancelAll()
{
    for (TouchPoint& point : _points) {
        point.delta = Vector2{};
        point.phase = TouchPhase::Cancelled;
    }
}

void TouchDevice::reset()
{
    _points.clear();
}

const TouchPoint* TouchDevice::getTouch(int index) const
{
    if (index < 0 || index >= static_cast<int>(_points.size())) {
        return nullptr;
    }
    return &_points[static_cast<size_t>(index)];
}

const TouchPoint* TouchDevice::getTouchById(int64_t id) const
{
    for (const TouchPoint& point : _points) {
        if (point.id == id) {
            return &point;
        }
    }
    return nullptr;
}

const TouchPoint* TouchDevice::getPrimaryTouch() const
{
    const TouchPoint* primary = nullptr;
    for (const TouchPoint& point : _points) {
        if (primary == nullptr || point.id < primary->id) {
            primary = &point;
        }
    }
    return primary;
}

} // namespace ayt::device
