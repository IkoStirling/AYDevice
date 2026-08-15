#pragma once
// AYDevice/TouchDevice.h - Multi-touch contact tracking with per-frame phases

#include "AYDevice/InputDevice.h"
#include "AYDevice/InputTypes.h"

#include <vector>

namespace ayt::device {

// Tracks active touch contacts and their per-frame phase/delta.
//
// Event flow per frame:
//   1. newFrame()   -> retire Ended/Cancelled points; mark survivors Stationary;
//                      clear per-frame deltas
//   2. onTouch(...) -> platform feeds this frame's contacts (Began/Moved/Ended)
//   3. queries      -> getTouchCount / getTouch / getTouchById
//
// Coordinates are window client space, matching MouseDevice.
class TouchDevice final : public IInputDevice {
public:
    TouchDevice();

    // ===== IInputDevice =====
    void newFrame() override;
    bool isConnected() const override { return true; }
    const char* deviceType() const override { return "Touch"; }

    // ===== Event feed (called by the platform layer) =====
    // A contact identified by id transitions to the given phase at (x, y).
    // Began adds it, Moved/Stationary updates it (delta accumulated), and
    // Ended/Cancelled marks it for retirement at the next newFrame().
    void onTouch(int64_t id, float x, float y, TouchPhase phase);
    void reset();

    // ===== Queries =====
    int getTouchCount() const { return static_cast<int>(_points.size()); }
    bool isTouched() const { return !_points.empty(); }

    const TouchPoint* getTouch(int index) const;
    const TouchPoint* getTouchById(int64_t id) const;

    // Primary contact = lowest active id; convenient for single-touch use.
    const TouchPoint* getPrimaryTouch() const;

private:
    TouchPoint* findById(int64_t id);

    std::vector<TouchPoint> _points;
};

} // namespace ayt::device
