#include "arp.h"

// Map a step number → index into the notes array, given the current order.
// Each order is either a closed-form formula (Up/Down/UpDown) or a fixed
// permutation table (Order1324). Adding a new order = adding one case here.
static uint8_t indexForStep(ArpOrder order, uint8_t step, uint8_t count) {
    if (count == 0) return 0;
    switch (order) {
        case ArpOrder::Up:
            return step % count;

        case ArpOrder::Down:
            return (uint8_t)((count - 1) - (step % count));

        case ArpOrder::UpDown: {
            // Palindrome that does NOT repeat endpoints: 0,1,2,3,2,1 (period = 2*(count-1))
            if (count <= 1) return 0;
            uint8_t period = (uint8_t)(2 * (count - 1));
            uint8_t phase  = step % period;
            return phase < count ? phase : (uint8_t)(period - phase);
        }

        case ArpOrder::Order1324: {
            // Skip pattern for a 4-note arp: indices 0, 2, 1, 3.
            // For arps with fewer than 4 notes this is undefined — fall back to Up.
            if (count < 4) return step % count;
            static const uint8_t pat[4] = {0, 2, 1, 3};
            return pat[step % 4];
        }

        default:
            return step % count;
    }
}

void Arp::setNotes(const uint8_t* notes, uint8_t count) {
    notes_ = notes;
    count_ = count;
    step_  = 0;
}

void Arp::setOrder(ArpOrder order) {
    order_ = order;
    step_  = 0;
}

void Arp::reset() {
    step_ = 0;
}

uint8_t Arp::nextNote() {
    if (notes_ == nullptr || count_ == 0) return 0;
    uint8_t idx = indexForStep(order_, step_, count_);
    step_++;
    return notes_[idx];
}
