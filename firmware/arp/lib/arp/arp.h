#pragma once
#include <cstdint>

enum class ArpOrder : uint8_t {
    Up = 0,
    Down,
    UpDown,
    Order1324,   // skip pattern; intended for 4-note arps. Falls back to Up if count < 4.
    COUNT
};

// Stateful arpeggiator. Holds a pointer to caller-owned notes; nextNote()
// advances internal step and returns the MIDI note for that step.
class Arp {
public:
    void setNotes(const uint8_t* notes, uint8_t count);
    void setOrder(ArpOrder order);
    void reset();
    uint8_t nextNote();

    ArpOrder order() const { return order_; }
    uint8_t  step()  const { return step_;  }
    uint8_t  count() const { return count_; }

private:
    const uint8_t* notes_ = nullptr;
    uint8_t        count_ = 0;
    uint8_t        step_  = 0;
    ArpOrder       order_ = ArpOrder::Up;
};
