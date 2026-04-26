#include "arp.h"
#include <cstdlib>

// Map a step number → index into the notes array, given the current order.
// Each order is either a closed-form formula (Up/Down/UpDownOpen/UpDownClosed)
// or a fixed permutation table (SkipUp), plus Random which uses std::rand().
// See decisions.md §24 for the open vs closed palindrome terminology.
// Adding a new order = adding one case here.
static uint8_t indexForStep(ArpOrder order, uint8_t step, uint8_t count) {
    if (count == 0) return 0;
    switch (order) {
        case ArpOrder::Up:
            return step % count;

        case ArpOrder::Down:
            return (uint8_t)((count - 1) - (step % count));

        case ArpOrder::UpDownOpen: {
            // Palindrome WITHOUT endpoint repeat: 0,1,2,3,2,1 (period 2*N-2).
            if (count <= 1) return 0;
            uint8_t period = (uint8_t)(2 * (count - 1));
            uint8_t phase  = step % period;
            return phase < count ? phase : (uint8_t)(period - phase);
        }

        case ArpOrder::UpDownClosed: {
            // Palindrome WITH endpoint repeat: 0,1,2,3,3,2,1,0 (period 2*N).
            // Both top and bottom are doubled — top in the middle of one
            // cycle, bottom across the loop boundary (… 1,0,0,1 …).
            if (count == 0) return 0;
            uint8_t period = (uint8_t)(2 * count);
            uint8_t phase  = step % period;
            return phase < count ? phase : (uint8_t)(period - 1 - phase);
        }

        case ArpOrder::SkipUp: {
            // Skip pattern for a 4-note arp: indices 0, 2, 1, 3.
            // For arps with fewer than 4 notes this is undefined — fall back to Up.
            if (count < 4) return step % count;
            static const uint8_t pat[4] = {0, 2, 1, 3};
            return pat[step % 4];
        }

        case ArpOrder::Random: {
            // Pure random with replacement. Caller is expected to seed
            // std::rand() (firmware does this in setup() from millis()/ADC
            // noise; tests call srand() explicitly for determinism).
            return (uint8_t)((unsigned)std::rand() % (unsigned)count);
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
