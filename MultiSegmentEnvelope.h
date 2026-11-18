#pragma once

#include "SimdAndMath.h"
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace duckflow {

enum class CurveType : int {
    Linear = 0,
    Tension2D = 1,     // Symmetric variable power bend (exact time-reversal symmetry!)
    SCurveSpline = 2,  // Smooth C2 quintic S-curve spline with symmetric tension bias
    SineWobble = 3,    // LFO-style sine wobble inside segment
    SawWobble = 4,     // LFO-style ramp/saw wobble inside segment
    StepHold = 5       // Hard rhythmic gate step
};

enum class EnvelopeTriggerLoopMode : int {
    OneShot = 0,       // Plays 0 -> 1 once per trigger and rests at final node value
    Loop = 1,          // Loops continuously while triggered
    LoopRelease = 2    // Loops [loopStart, loopEnd] while note/gate held, then plays release to 1.0
};

struct EnvelopeNode {
    float x = 0.0f;                 // Normalized time [0.0, 1.0]
    float y = 1.0f;                 // Gain level [0.0, 1.0] (1.0 = full volume, 0.0 = full duck)
    float tension = 0.0f;           // [-1.0, 1.0] curve bend or wobble density
    CurveType curve = CurveType::SCurveSpline;

    bool operator==(const EnvelopeNode& o) const noexcept {
        return std::fabs(x - o.x) < 1e-6f &&
               std::fabs(y - o.y) < 1e-6f &&
               std::fabs(tension - o.tension) < 1e-6f &&
               curve == o.curve;
    }
};

struct EnvelopeData {
    std::vector<EnvelopeNode> nodes;
    float loopStart = 0.25f;
    float loopEnd = 0.75f;

    EnvelopeData() {
        setDefaultSidechainPump();
    }

    void sortAndValidate() {
        if (nodes.size() < 2) {
            setDefaultSidechainPump();
            return;
        }
        for (auto& n : nodes) {
            n.x = std::clamp(n.x, 0.0f, 1.0f);
            n.y = std::clamp(n.y, 0.0f, 1.0f);
            n.tension = std::clamp(n.tension, -1.0f, 1.0f);
        }
        std::stable_sort(nodes.begin(), nodes.end(),
                         [](const EnvelopeNode& a, const EnvelopeNode& b) { return a.x < b.x; });
        nodes.front().x = 0.0f;
        nodes.back().x = 1.0f;

        loopStart = std::clamp(loopStart, 0.0f, 0.95f);
        loopEnd = std::clamp(loopEnd, loopStart + 0.05f, 1.0f);
    }

    void setDefaultSidechainPump() {
        nodes.clear();
        nodes.push_back({0.0f, 0.0f, 0.0f, CurveType::Tension2D});
        nodes.push_back({0.06f, 0.0f, -0.35f, CurveType::SCurveSpline});
        nodes.push_back({0.52f, 1.0f, 0.0f, CurveType::Linear});
        nodes.push_back({1.0f, 1.0f, 0.0f, CurveType::Linear});
        loopStart = 0.06f;
        loopEnd = 0.52f;
    }

    static float applySwing(float phase, float swingNormalized) noexcept {
        const float p = std::clamp(phase, 0.0f, 1.0f);
        if (std::fabs(swingNormalized) < 1.0e-4f) return p;
        const float mid = std::clamp(0.5f + swingNormalized * 0.33f, 0.15f, 0.85f);
        if (p < mid) {
            return 0.5f * (p / mid);
        }
        return 0.5f + 0.5f * ((p - mid) / (1.0f - mid));
    }

    /**
     * Time-reversal symmetric bend warp g(t, k):
     * Strictly satisfies 1 - g(1 - t, -k) === g(t, k) for all t in [0,1] and k in [-1,1],
     * guaranteeing lossless curve geometry under Reverse operations!
     */
    static inline float symmetricBendWarp(float t, float k, float scaleExp = 1.15f) noexcept {
        if (std::fabs(k) <= 1.0e-4f) return t;
        const float p = std::pow(10.0f, std::fabs(k) * scaleExp);
        if (k >= 0.0f) {
            return std::pow(t, p);
        }
        return 1.0f - std::pow(1.0f - t, p);
    }

    static float evalSegment(float t, float y0, float y1, CurveType type, float tension) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (type) {
            case CurveType::Linear:
                return y0 + (y1 - y0) * t;

            case CurveType::StepHold:
                return (t < 1.0f) ? y0 : y1;

            case CurveType::Tension2D: {
                const float k = std::clamp(tension, -0.98f, 0.98f);
                const float shaped = symmetricBendWarp(t, k, 1.15f);
                return y0 + (y1 - y0) * shaped;
            }

            case CurveType::SCurveSpline: {
                const float k = std::clamp(tension, -0.95f, 0.95f);
                const float tb = symmetricBendWarp(t, k, 0.65f);
                const float s = tb * tb * tb * (tb * (tb * 6.0f - 15.0f) + 10.0f);
                return y0 + (y1 - y0) * s;
            }

            case CurveType::SineWobble: {
                const int cycles = 1 + static_cast<int>(std::round((tension * 0.5f + 0.5f) * 7.0f));
                const float base = y0 + (y1 - y0) * t;
                const float amp = std::min(base, 1.0f - base);
                const float mod = 0.5f * (1.0f - std::cos(static_cast<float>(kTwoPi) * static_cast<float>(cycles) * t));
                if (amp > 0.02f) {
                    return std::clamp(base + (mod - 0.5f) * 2.0f * amp, 0.0f, 1.0f);
                }
                const float target = (y0 >= 0.5f) ? (1.0f - mod) : mod;
                return std::clamp(target, 0.0f, 1.0f);
            }

            case CurveType::SawWobble: {
                const int cycles = 1 + static_cast<int>(std::round((tension * 0.5f + 0.5f) * 7.0f));
                const float cycPhase = std::fmod(t * static_cast<float>(cycles), 1.0f);
                const float env = y0 + (y1 - y0) * t;
                return std::clamp(cycPhase * std::max(0.15f, env), 0.0f, 1.0f);
            }
        }
        return y0 + (y1 - y0) * t;
    }

    float evaluate(float rawPhase, float swing = 0.0f, bool exactEndInOneShot = false) const noexcept {
        if (nodes.empty()) return 1.0f;
        if (nodes.size() == 1) return nodes.front().y;

        if (exactEndInOneShot && rawPhase >= 1.0f) {
            return nodes.back().y;
        }

        const float x = applySwing(std::clamp(rawPhase, 0.0f, 1.0f), swing);
        if (x <= nodes.front().x) return nodes.front().y;
        if (x >= nodes.back().x) return nodes.back().y;

        std::size_t idx = 0;
        for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
            if (x >= nodes[i].x && x <= nodes[i + 1].x) {
                idx = i;
                if (nodes[i + 1].x > x) break;
            }
        }

        const auto& n0 = nodes[idx];
        const auto& n1 = nodes[idx + 1];
        const float dx = n1.x - n0.x;
        if (dx <= 1.0e-6f) {
            return n1.y;
        }
        const float localT = (x - n0.x) / dx;
        return std::clamp(evalSegment(localT, n0.y, n1.y, n0.curve, n0.tension), 0.0f, 1.0f);
    }

    void invert() {
        for (auto& n : nodes) n.y = 1.0f - n.y;
    }

    void reverse() {
        const std::size_t count = nodes.size();
        if (count < 2) return;
        std::vector<EnvelopeNode> rev(count);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t src = count - 1 - i;
            rev[i].x = 1.0f - nodes[src].x;
            rev[i].y = nodes[src].y;
            if (src > 0) {
                rev[i].curve = nodes[src - 1].curve;
                // Exact time-reversal negation for Tension2D and SCurveSpline; keep cycle count for wobbles
                if (rev[i].curve == CurveType::Tension2D || rev[i].curve == CurveType::SCurveSpline) {
                    rev[i].tension = -nodes[src - 1].tension;
                } else {
                    rev[i].tension = nodes[src - 1].tension;
                }
            } else {
                rev[i].curve = CurveType::Linear;
                rev[i].tension = 0.0f;
            }
        }
        const float oldLs = loopStart;
        const float oldLe = loopEnd;
        loopStart = 1.0f - oldLe;
        loopEnd = 1.0f - oldLs;
        nodes = std::move(rev);
        sortAndValidate();
    }

    void rotatePhase(float deltaX) {
        if (nodes.size() < 2) return;
        const float shift = std::fmod(deltaX + 10.0f, 1.0f);
        if (shift < 1e-5f) return;

        const float splitX = std::fmod(1.0f - shift, 1.0f);
        const float splitY = evaluate(splitX);

        std::vector<EnvelopeNode> rotated;
        rotated.push_back({0.0f, splitY, 0.0f, CurveType::SCurveSpline});
        for (std::size_t i = 1; i + 1 < nodes.size(); ++i) {
            EnvelopeNode n = nodes[i];
            n.x = std::fmod(n.x + shift, 1.0f);
            rotated.push_back(n);
        }
        rotated.push_back({1.0f, splitY, 0.0f, CurveType::Linear});
        nodes = std::move(rotated);
        sortAndValidate();
    }

    void duplicateDoubleTime() {
        if (nodes.size() < 2) return;
        std::vector<EnvelopeNode> dup;
        dup.reserve(nodes.size() * 2);
        for (const auto& n : nodes) {
            EnvelopeNode a = n;
            a.x *= 0.5f;
            dup.push_back(a);
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            EnvelopeNode b = nodes[i];
            b.x = 0.5f + b.x * 0.5f;
            dup.push_back(b);
        }
        nodes = std::move(dup);
        sortAndValidate();
    }

    void halveTime() {
        if (nodes.size() < 2) return;
        const float midY = evaluate(0.5f);
        std::vector<EnvelopeNode> half;
        for (const auto& n : nodes) {
            if (n.x < 0.5f - 1e-5f) {
                EnvelopeNode h = n;
                h.x *= 2.0f;
                half.push_back(h);
            }
        }
        half.push_back({1.0f, midY, 0.0f, CurveType::Linear});
        if (half.front().x > 0.0f) {
            half.insert(half.begin(), {0.0f, evaluate(0.0f), 0.0f, CurveType::SCurveSpline});
        }
        nodes = std::move(half);
        sortAndValidate();
    }

    void randomize(std::uint32_t& rngState, int gridSteps = 8) {
        auto nextFloat = [&rngState]() -> float {
            rngState ^= rngState << 13;
            rngState ^= rngState >> 17;
            rngState ^= rngState << 5;
            return static_cast<float>(rngState & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
        };

        gridSteps = std::clamp(gridSteps, 4, 32);
        nodes.clear();
        const float startY = (nextFloat() < 0.65f) ? 0.0f : std::round(nextFloat() * 4.0f) * 0.25f;
        nodes.push_back({0.0f, startY, (nextFloat() - 0.5f) * 1.2f, CurveType::SCurveSpline});

        for (int s = 1; s < gridSteps; ++s) {
            if (nextFloat() < 0.6f) {
                const float x = static_cast<float>(s) / static_cast<float>(gridSteps);
                const float y = std::clamp(std::round(nextFloat() * 8.0f) / 8.0f, 0.0f, 1.0f);
                const float ten = (nextFloat() - 0.5f) * 1.4f;
                const int cPick = static_cast<int>(nextFloat() * 4.0f);
                CurveType ct = CurveType::SCurveSpline;
                if (cPick == 1) ct = CurveType::Tension2D;
                else if (cPick == 2) ct = CurveType::Linear;
                else if (cPick == 3) ct = CurveType::StepHold;
                nodes.push_back({x, y, ten, ct});
            }
        }
        nodes.push_back({1.0f, 1.0f, 0.0f, CurveType::Linear});
        sortAndValidate();
    }

    void paintSegment(float xStart, float xEnd, float yStart, float yEnd, CurveType brushCurve, float tension = 0.0f) {
        if (xEnd < xStart) {
            std::swap(xStart, xEnd);
            std::swap(yStart, yEnd);
        }
        xStart = std::clamp(xStart, 0.0f, 1.0f);
        xEnd = std::clamp(xEnd, 0.0f, 1.0f);
        if (xEnd - xStart < 0.005f) return;

        const float preY = evaluate(std::max(0.0f, xStart - 0.0005f));
        const float postY = evaluate(std::min(1.0f, xEnd + 0.0005f));

        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const EnvelopeNode& n) {
            return n.x >= xStart - 0.0002f && n.x <= xEnd + 0.0002f;
        }), nodes.end());

        if (xStart > 0.001f) {
            nodes.push_back({xStart, preY, 0.0f, CurveType::StepHold});
        }
        nodes.push_back({xStart, std::clamp(yStart, 0.0f, 1.0f), tension, brushCurve});
        nodes.push_back({xEnd, std::clamp(yEnd, 0.0f, 1.0f), 0.0f, CurveType::StepHold});
        if (xEnd < 0.999f) {
            nodes.push_back({xEnd, postY, 0.0f, CurveType::SCurveSpline});
        }
        if (nodes.empty() || nodes.front().x > 0.0f) {
            nodes.insert(nodes.begin(), {0.0f, preY, 0.0f, CurveType::Linear});
        }
        if (nodes.back().x < 1.0f) {
            nodes.push_back({1.0f, postY, 0.0f, CurveType::Linear});
        }
        sortAndValidate();
    }

    std::string serialize() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(5);
        oss << loopStart << "," << loopEnd << ";";
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (i > 0) oss << "|";
            oss << nodes[i].x << "," << nodes[i].y << "," << nodes[i].tension << "," << static_cast<int>(nodes[i].curve);
        }
        return oss.str();
    }

    bool deserialize(const std::string& text) {
        if (text.empty()) return false;
        const auto semi = text.find(';');
        if (semi == std::string::npos) return false;

        EnvelopeData parsed;
        parsed.nodes.clear();

        {
            std::istringstream hdr(text.substr(0, semi));
            char comma = 0;
            if (!(hdr >> parsed.loopStart >> comma >> parsed.loopEnd)) return false;
        }

        std::istringstream body(text.substr(semi + 1));
        std::string token;
        while (std::getline(body, token, '|')) {
            if (token.empty()) continue;
            std::istringstream ns(token);
            EnvelopeNode n;
            int curveInt = 0;
            char c1 = 0, c2 = 0, c3 = 0;
            if (ns >> n.x >> c1 >> n.y >> c2 >> n.tension >> c3 >> curveInt) {
                n.curve = static_cast<CurveType>(std::clamp(curveInt, 0, 5));
                parsed.nodes.push_back(n);
            }
        }
        if (parsed.nodes.size() < 2) return false;
        parsed.sortAndValidate();
        *this = std::move(parsed);
        return true;
    }
};

class EnvelopeUndoHistory {
public:
    static constexpr std::size_t kMaxHistory = 64;

    void reset(const EnvelopeData& initial) {
        stack_.clear();
        stack_.push_back(initial);
        index_ = 0;
    }

    void push(const EnvelopeData& state) {
        if (!stack_.empty() && stack_[index_].nodes == state.nodes &&
            std::fabs(stack_[index_].loopStart - state.loopStart) < 1e-5f &&
            std::fabs(stack_[index_].loopEnd - state.loopEnd) < 1e-5f) {
            return;
        }
        if (index_ + 1 < stack_.size()) {
            stack_.erase(stack_.begin() + static_cast<std::ptrdiff_t>(index_ + 1), stack_.end());
        }
        stack_.push_back(state);
        if (stack_.size() > kMaxHistory) {
            stack_.erase(stack_.begin());
        }
        index_ = stack_.size() - 1;
    }

    bool canUndo() const noexcept { return index_ > 0 && !stack_.empty(); }
    bool canRedo() const noexcept { return !stack_.empty() && index_ + 1 < stack_.size(); }

    bool undo(EnvelopeData& out) {
        if (!canUndo()) return false;
        --index_;
        out = stack_[index_];
        return true;
    }

    bool redo(EnvelopeData& out) {
        if (!canRedo()) return false;
        ++index_;
        out = stack_[index_];
        return true;
    }

private:
    std::vector<EnvelopeData> stack_;
    std::size_t index_ = 0;
};

} // namespace duckflow
