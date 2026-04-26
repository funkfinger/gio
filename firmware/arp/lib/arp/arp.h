#pragma once
#include <cstdint>

// See decisions.md §24 for the open vs closed palindrome terminology.
enum class ArpOrder : uint8_t {
    Up = 0,
    Down,
    UpDownClosed,    // palindrome WITH endpoint repeat:    0,1,2,3,3,2,1,0 (period 2*N)
    UpDownOpen,      // palindrome WITHOUT endpoint repeat: 0,1,2,3,2,1     (period 2*N-2)
    SkipUp,          // permutation 1-3-2-4 for 4-note arps. Falls back to Up if count < 4.
    Random,          // pure random index per step (with replacement). Uses std::rand().
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
