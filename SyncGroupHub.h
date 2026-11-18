#pragma once

#include "MultiSegmentEnvelope.h"
#include <array>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace duckflow {

/**
 * SyncGroupHub
 * Implements Duck 2's 4-Group cross-instance envelope synchronization:
 *  - Groups 1..4 (displayed as Group A, B, C, D; 0 = Off)
 *  - Any plugin instance assigned to Group G automatically broadcasts envelope edits
 *    to all sibling instances in the same DAW process without blocking the audio thread.
 */
class SyncGroupHub {
public:
    static constexpr int kNumGroups = 4;

    struct GroupSlot {
        mutable std::mutex mutex;
        std::array<EnvelopeData, 3> bandEnvelopes{};
        bool unlinkBands = false;
        std::atomic<std::uint64_t> revision{0};
    };

    static SyncGroupHub& instance() {
        static SyncGroupHub hub;
        return hub;
    }

    void publish(int groupIndex, const std::array<EnvelopeData, 3>& envs, bool unlinkBands, std::uint64_t& callerRev) {
        if (groupIndex < 1 || groupIndex > kNumGroups) return;
        auto& slot = slots_[static_cast<std::size_t>(groupIndex - 1)];
        std::lock_guard<std::mutex> lock(slot.mutex);
        slot.bandEnvelopes = envs;
        slot.unlinkBands = unlinkBands;
        const std::uint64_t nextRev = slot.revision.fetch_add(1, std::memory_order_acq_rel) + 1;
        callerRev = nextRev;
    }

    bool pollIfUpdated(int groupIndex, std::uint64_t& lastSeenRev, std::array<EnvelopeData, 3>& outEnvs, bool& outUnlink) const {
        if (groupIndex < 1 || groupIndex > kNumGroups) return false;
        const auto& slot = slots_[static_cast<std::size_t>(groupIndex - 1)];
        const std::uint64_t curRev = slot.revision.load(std::memory_order_acquire);
        if (curRev == 0 || curRev == lastSeenRev) {
            return false;
        }
        std::lock_guard<std::mutex> lock(slot.mutex);
        outEnvs = slot.bandEnvelopes;
        outUnlink = slot.unlinkBands;
        lastSeenRev = slot.revision.load(std::memory_order_relaxed);
        return true;
    }

private:
    SyncGroupHub() = default;
    std::array<GroupSlot, kNumGroups> slots_{};
};

} // namespace duckflow
