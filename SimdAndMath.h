#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

namespace duckflow {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr float kMinGainDb = -96.0f;

class ScopedDenormalFlush {
public:
    ScopedDenormalFlush() noexcept {
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
        prevMxcsr_ = _mm_getcsr();
        _mm_setcsr(prevMxcsr_ | 0x8040u);
#elif defined(__aarch64__)
        std::uint64_t fpcr = 0;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        prevFpcr_ = fpcr;
        fpcr |= (1ULL << 24);
        asm volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
    }

    ~ScopedDenormalFlush() noexcept {
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
        _mm_setcsr(prevMxcsr_);
#elif defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(prevFpcr_));
#endif
    }

    ScopedDenormalFlush(const ScopedDenormalFlush&) = delete;
    ScopedDenormalFlush& operator=(const ScopedDenormalFlush&) = delete;

private:
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    unsigned int prevMxcsr_ = 0;
#elif defined(__aarch64__)
    std::uint64_t prevFpcr_ = 0;
#endif
};

inline float sanitizeSample(float x) noexcept {
    if (!std::isfinite(x) || std::fabs(x) < 1.0e-20f) {
        return 0.0f;
    }
    return std::clamp(x, -16.0f, 16.0f);
}

inline float dbToGain(float db) noexcept {
    if (db <= kMinGainDb) return 0.0f;
    return std::pow(10.0f, db * 0.05f);
}

inline float gainToDb(float gain) noexcept {
    if (gain <= 1.0e-5f) return kMinGainDb;
    return 20.0f * std::log10(gain);
}

/**
 * Critically-damped 2nd-order smooth gain filter with cached coefficient calculation
 * so variable 1-sample micro-buffers (e.g. FL Studio automation splits) have near-zero overhead.
 */
class CriticallyDampedSmoother {
public:
    void prepare(double sampleRate, float smoothingTimeMs) noexcept {
        sampleRate_ = std::max(8000.0, sampleRate);
        lastTimeMs_ = -1.0f;
        setTimeMs(smoothingTimeMs);
        reset(1.0f);
    }

    void setTimeMs(float smoothingTimeMs) noexcept {
        const float clampedMs = std::clamp(smoothingTimeMs, 0.05f, 100.0f);
        if (std::fabs(clampedMs - lastTimeMs_) < 1.0e-4f) {
            return; // Cached: zero std::exp overhead when unchanged across micro-buffers!
        }
        lastTimeMs_ = clampedMs;
        const double tau = (static_cast<double>(clampedMs) * 0.001) * 0.35;
        if (tau <= 1.0e-6) {
            coeff_ = 1.0f;
        } else {
            coeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (tau * sampleRate_)));
            coeff_ = std::clamp(coeff_, 0.00001f, 1.0f);
        }
    }

    void reset(float value = 1.0f) noexcept {
        z1_ = value;
        z2_ = value;
    }

    inline float process(float target) noexcept {
        z1_ += coeff_ * (target - z1_);
        z2_ += coeff_ * (z1_ - z2_);
        return z2_;
    }

    float current() const noexcept { return z2_; }

private:
    double sampleRate_ = 48000.0;
    float lastTimeMs_ = -1.0f;
    float coeff_ = 0.1f;
    float z1_ = 1.0f;
    float z2_ = 1.0f;
};

} // namespace duckflow
