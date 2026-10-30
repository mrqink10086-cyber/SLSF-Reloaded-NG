#pragma once

// Independent, deliberately simple scheduler model for differential
// verification of slsf::time::Scheduler.  Lives only in test
// targets: the flat record list trades speed for obviously-correct ordering
// and cancellation semantics.

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace refsched {

using DomainId = std::uint64_t; // 0=Game, 1=Real, 2=Active

struct Record {
    std::uint64_t sequence{};
    DomainId domain{};
    double deadline{0.0};
    std::uint64_t generation{1}; // bumped on cancel; token match dies after
    bool retired{false};         // cancelled or consumed
};

class ReferenceScheduler {
public:
    // Returns (record index, generation as handed out to callers).
    std::pair<std::size_t, std::uint64_t> Schedule(DomainId domain, double deadline) {
        Record record;
        record.sequence = nextSequence_++;
        record.domain = domain;
        record.deadline = deadline;
        records_.push_back(record);
        return {records_.size() - 1, records_.back().generation};
    }

    bool Cancel(std::size_t index, std::uint64_t generation) {
        if (index >= records_.size()) {
            return false;
        }
        auto& record = records_[index];
        if (record.retired || record.generation != generation) {
            return false;
        }
        record.retired = true;
        ++record.generation;
        return true;
    }

    void NewSession() {
        for (auto& record : records_) {
            if (!record.retired) {
                record.retired = true;
                ++record.generation;
            }
        }
        ++session_;
    }

    std::uint64_t Session() const { return session_; }

    // Due record indices on `domain` in (deadline, sequence) order; the
    // records are consumed, later AcquireDue calls no longer see them.
    std::vector<std::size_t> AcquireDue(DomainId domain, double now) {
        std::vector<std::size_t> due;
        for (std::size_t i = 0; i < records_.size(); ++i) {
            const auto& record = records_[i];
            if (record.domain != domain || record.retired || record.deadline > now) {
                continue;
            }
            due.push_back(i);
        }
        std::stable_sort(due.begin(), due.end(), [&](std::size_t a, std::size_t b) {
            if (records_[a].deadline != records_[b].deadline) {
                return records_[a].deadline < records_[b].deadline;
            }
            return records_[a].sequence < records_[b].sequence;
        });
        for (auto index : due) {
            records_[index].retired = true;
        }
        return due;
    }

    std::size_t PendingCount(DomainId domain) const {
        std::size_t count = 0;
        for (const auto& record : records_) {
            count += (record.domain == domain && !record.retired) ? 1 : 0;
        }
        return count;
    }

private:
    std::vector<Record> records_;
    std::uint64_t nextSequence_{0};
    std::uint64_t session_{1};
};

}