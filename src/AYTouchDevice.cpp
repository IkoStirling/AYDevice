#include "AYDevice/TouchDevice.h"

#include <algorithm>

namespace ayt::device {

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
