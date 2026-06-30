#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace FluentUI {

    // brief 10 Part B: free helper (defined in Context.cpp) that scales a base
    // duration by g_ctx->motion.durationScale and returns 0 when reduceMotion is
    // on. Declared here (not via #include "Context.h") to avoid the Animation.h ↔
    // Context.h include cycle: Context.h already includes Animation.h. AnimatedValue
    // and SpringValue route their durations through it so the global MotionConfig is
    // the single point that scales / disables motion. Returns base unchanged when no
    // context exists (e.g. unit tests constructing AnimatedValue in isolation).
    float MotionDuration(float base);

    // Perf 2.1: Easing type enum (1 byte) replaces std::function (32+ bytes)
    enum class EasingType : uint8_t {
        Linear = 0,
        EaseIn,
        EaseOut,
        EaseInOut,
        EaseOutCubic,
        EaseOutExpo,
        // brief 10 Part B: Fluent/WinUI cubic-bezier motion curves (mapped to
        // Easing::CubicBezier in Evaluate below).
        Standard,    // (0.8, 0.0, 0.2, 1.0) — general purpose move
        Decelerate,  // (0.1, 0.9, 0.2, 1.0) — entrances (ease-out feel)
        Accelerate   // (0.7, 0.0, 1.0, 0.5) — exits (ease-in feel)
    };

    // Funciones de easing para animaciones suaves (Fluent Design)
    namespace Easing {

        inline float EaseOut(float t) {
            return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        }

        inline float EaseInOut(float t) {
            return t < 0.5f
                ? 2.0f * t * t
                : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
        }

        inline float EaseIn(float t) {
            return t * t * t;
        }

        inline float Linear(float t) {
            return t;
        }

        inline float EaseOutCubic(float t) {
            float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        inline float EaseOutExpo(float t) {
            return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
        }

        // brief 10 Part B: CSS-style cubic-bezier (control points (x1,y1),(x2,y2);
        // implicit endpoints (0,0) and (1,1)). Solves x(t)=u for the curve parameter
        // t with ~5 Newton iterations (clamped), then returns y(t). Used to express
        // the Fluent motion tokens (Standard/Decelerate/Accelerate).
        inline float CubicBezier(float x1, float y1, float x2, float y2, float x) {
            auto cx = [&](float t){ float u = 1.0f - t; return 3.0f*u*u*t*x1 + 3.0f*u*t*t*x2 + t*t*t; };
            auto cy = [&](float t){ float u = 1.0f - t; return 3.0f*u*u*t*y1 + 3.0f*u*t*t*y2 + t*t*t; };
            x = std::clamp(x, 0.0f, 1.0f);
            float t = x;
            for (int i = 0; i < 5; ++i) {
                float xe = cx(t) - x;
                float d = (cx(t + 1e-3f) - cx(t - 1e-3f)) / 2e-3f;
                if (std::abs(d) < 1e-6f) break;
                t -= xe / d;
                t = std::clamp(t, 0.0f, 1.0f);
            }
            return cy(t);
        }

        // Perf 2.1: Evaluate easing by enum (no function pointer overhead)
        inline float Evaluate(EasingType type, float t) {
            switch (type) {
                case EasingType::Linear:       return t;
                case EasingType::EaseIn:       return t * t * t;
                case EasingType::EaseOut:       { float u = 1.0f - t; return 1.0f - u * u * u; }
                case EasingType::EaseInOut:    return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
                case EasingType::EaseOutCubic: { float u = 1.0f - t; return 1.0f - u * u * u; }
                case EasingType::EaseOutExpo:  return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
                // brief 10 Part B: Fluent motion tokens via cubic-bezier.
                case EasingType::Standard:     return CubicBezier(0.8f, 0.0f, 0.2f, 1.0f, t);
                case EasingType::Decelerate:   return CubicBezier(0.1f, 0.9f, 0.2f, 1.0f, t);
                case EasingType::Accelerate:   return CubicBezier(0.7f, 0.0f, 1.0f, 0.5f, t);
                default: return t;
            }
        }
    }

    // brief 10 Part B: global motion tokens. Accessible as g_ctx->motion. The single
    // point of policy: durationScale stretches every animation duration (debug /
    // accessibility preference); reduceMotion collapses durations to 0 and degrades
    // springs to immediate snaps; enabled is a master switch hosts can flip.
    struct MotionConfig {
        float durationScale = 1.0f;
        bool  reduceMotion  = false;
        bool  enabled       = true;
    };

    // Clase para animar valores con transiciones suaves
    template<typename T>
    class AnimatedValue {
    public:
        AnimatedValue() : current_(T{}), target_(T{}), duration_(0.3f), elapsed_(0.0f), active_(false), initialized_(false) {}

        AnimatedValue(const T& initial)
            : current_(initial), target_(initial), duration_(0.3f), elapsed_(0.0f), active_(false), initialized_(true) {}

        // Perf 2.1: SetTarget with EasingType enum (backward compatible overload below)
        // brief 10 Part B: duration is funneled through MotionDuration so the global
        // MotionConfig scales/disables it. A reduceMotion=0 duration snaps in Update.
        void SetTarget(const T& target, float duration, EasingType easing) {
            if (target_ != target) {
                start_ = current_;
                target_ = target;
                duration_ = MotionDuration(duration);
                elapsed_ = 0.0f;
                active_ = true;
                easingType_ = easing;
            }
        }

        // brief 10 Part F (stagger): delay (seconds) before this animation begins
        // advancing. Update() counts it down first, so a container can offset each
        // child's entrance by index*staggerMs to produce a cascade. Set AFTER
        // SetTarget (which resets the clock). Scaled by MotionConfig via the caller.
        void SetDelay(float seconds) { delay_ = (seconds > 0.0f) ? seconds : 0.0f; }
        float GetDelay() const { return delay_; }

        // Backward-compatible overload: std::function<float(float)> mapped to enum
        void SetTarget(const T& target, float duration = 0.3f,
                      float(*easing)(float) = Easing::EaseOutCubic) {
            // Map known function pointers to enum
            EasingType type = EasingType::EaseOutCubic;
            if (easing == Easing::Linear) type = EasingType::Linear;
            else if (easing == Easing::EaseIn) type = EasingType::EaseIn;
            else if (easing == Easing::EaseOut) type = EasingType::EaseOut;
            else if (easing == Easing::EaseInOut) type = EasingType::EaseInOut;
            else if (easing == Easing::EaseOutExpo) type = EasingType::EaseOutExpo;
            SetTarget(target, duration, type);
        }

        // Actualizar animación (llamar cada frame)
        void Update(float deltaTime) {
            if (!active_) return;

            // brief 10 Part F: burn off the stagger delay first (with any leftover dt
            // carried into the first real step so timing stays exact).
            if (delay_ > 0.0f) {
                delay_ -= deltaTime;
                if (delay_ > 0.0f) return;
                deltaTime = -delay_;   // leftover after the delay elapsed this frame
                delay_ = 0.0f;
            }

            // brief 10 Part B: a 0 duration (reduceMotion) snaps to the target.
            if (duration_ <= 1e-4f) {
                current_ = target_;
                active_ = false;
                return;
            }

            elapsed_ += deltaTime;
            float t = std::min(elapsed_ / duration_, 1.0f);
            float eased = Easing::Evaluate(easingType_, t);

            current_ = Lerp(start_, target_, eased);

            if (t >= 1.0f) {
                current_ = target_;
                active_ = false;
            }
        }

        // Obtener valor actual
        const T& Get() const { return current_; }
        operator const T&() const { return current_; }

        // Verificar si está animando
        bool IsAnimating() const { return active_; }

        // Forzar valor sin animación
        void SetImmediate(const T& value) {
            current_ = value;
            target_ = value;
            active_ = false;
            initialized_ = true;
        }

        // Check if this animation has been initialized with a value
        bool IsInitialized() const { return initialized_; }

    private:
        T Lerp(const T& a, const T& b, float t) {
            if constexpr (std::is_arithmetic_v<T>) {
                return a + (b - a) * t;
            } else {
                return a + (b - a) * t;
            }
        }

        T current_;
        T target_;
        T start_;
        float duration_;
        float elapsed_;
        bool active_;
        bool initialized_;
        float delay_ = 0.0f; // brief 10 Part F: stagger delay (seconds), counted down in Update
        EasingType easingType_ = EasingType::EaseOutCubic; // Perf 2.1: 1 byte vs 32+ bytes
    };

    // Especialización para Color
    template<>
    inline Color AnimatedValue<Color>::Lerp(const Color& a, const Color& b, float t) {
        return Color(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        );
    }

    // Especialización para Vec2
    template<>
    inline Vec2 AnimatedValue<Vec2>::Lerp(const Vec2& a, const Vec2& b, float t) {
        return Vec2(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
        );
    }

    // ─── brief 10 Part A — SpringValue<T> (springs interrumpibles) ────────────────
    // Critically-damped-by-default mass-spring. Unlike AnimatedValue, re-targeting
    // mid-flight preserves the current velocity_ instead of resetting a clock, so a
    // fast hover-in/out reverses continuously (no "kick"). Integrated with a
    // semi-implicit (symplectic) Euler stepper, sub-stepped at ≤1/120 s so a slow
    // frame can't blow the spring up. Specializations for Add/Sub/Scale/NearlyZero/
    // Zero (below) make it usable with float, Color and Vec2 — the generic body uses
    // operator +/-/* which Color and Vec2 already provide, but NearlyZero and the
    // velocity zero need per-type handling (Color's default ctor has a=1, which would
    // otherwise seed a non-zero alpha velocity).
    template<typename T>
    class SpringValue {
    public:
        SpringValue() = default;
        explicit SpringValue(const T& initial)
            : current_(initial), target_(initial), initialized_(true) {}

        // omega (natural frequency) and zeta (damping ratio) are derived from a
        // "response" (≈ settle time, s) and a dampingRatio (1=critical, <1 overshoot).
        void Configure(float response, float dampingRatio = 1.0f) {
            const float pi = 3.14159265358979323846f;
            omega_ = (response > 1e-4f) ? (2.0f * pi / response) : 60.0f;
            zeta_  = dampingRatio;
        }

        // Interruptible: keeps velocity_. First target initializes in place (no anim).
        // brief 10 Part B: reduceMotion (MotionDuration(1)==0) degrades to a snap.
        void SetTarget(const T& target) {
            if (!initialized_) { current_ = target; target_ = target; initialized_ = true; active_ = false; return; }
            if (MotionDuration(1.0f) <= 1e-4f) { SetImmediate(target); return; }
            target_ = target;
            active_ = true;
        }
        void SetImmediate(const T& v) { current_ = v; target_ = v; velocity_ = Zero(); active_ = false; initialized_ = true; }

        // brief 10 Part E (FLIP): inject an instantaneous offset into the current
        // value while leaving target_ unchanged, preserving velocity_ so successive
        // bumps compose naturally. With target_ == Zero() the spring then decays the
        // offset back to 0, sliding an item from its old position to the new one.
        void Nudge(const T& delta) {
            if (!initialized_) { current_ = Zero(); target_ = Zero(); initialized_ = true; }
            current_ = Add(current_, delta);
            active_ = true;
        }

        void Update(float dt) {
            if (!active_) return;
            dt = std::min(dt, 0.064f);                 // clamp a long frame
            const float h = 1.0f / 120.0f;
            int steps = std::max(1, (int)std::ceil(dt / h));
            float sdt = dt / (float)steps;
            for (int i = 0; i < steps; ++i) {
                // a = -omega^2 (x - target) - 2*zeta*omega*v
                T disp  = Sub(current_, target_);
                T accel = Sub(Scale(disp, -omega_ * omega_), Scale(velocity_, 2.0f * zeta_ * omega_));
                velocity_ = Add(velocity_, Scale(accel, sdt));    // v += a*dt
                current_  = Add(current_,  Scale(velocity_, sdt)); // x += v*dt (uses updated v)
            }
            if (NearlyZero(Sub(current_, target_)) && NearlyZero(velocity_)) {
                current_ = target_; velocity_ = Zero(); active_ = false;
            }
        }

        const T& Get() const { return current_; }
        operator const T&() const { return current_; }
        bool IsAnimating() const { return active_; }
        bool IsInitialized() const { return initialized_; }

    private:
        // Generic vector ops (Color/Vec2 supply +,-,*float). Specialized below where
        // the generic form can't compile or is wrong for the type.
        static T Add(const T& a, const T& b)  { return a + b; }
        static T Sub(const T& a, const T& b)  { return a - b; }
        static T Scale(const T& a, float s)   { return a * s; }
        static bool NearlyZero(const T& a)    { return std::abs((float)a) < 0.001f; }
        static T Zero()                       { return T{}; }

        T current_{}, target_{};
        T velocity_ = Zero();
        float omega_ = 30.0f, zeta_ = 1.0f;
        bool active_ = false, initialized_ = false;
    };

    // float: generic Add/Sub/Scale are fine; NearlyZero/Zero too (kept explicit for
    // clarity / to mirror the other specializations).
    template<> inline float SpringValue<float>::Zero() { return 0.0f; }
    template<> inline bool  SpringValue<float>::NearlyZero(const float& a) { return std::abs(a) < 0.001f; }

    // Color: velocity is a 4-float vector. Zero must be (0,0,0,0) — NOT Color{} which
    // defaults alpha to 1 (that would seed a phantom alpha velocity). NearlyZero
    // checks all four channels.
    template<> inline Color SpringValue<Color>::Zero() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
    template<> inline bool  SpringValue<Color>::NearlyZero(const Color& a) {
        return std::abs(a.r) < 0.0015f && std::abs(a.g) < 0.0015f &&
               std::abs(a.b) < 0.0015f && std::abs(a.a) < 0.0015f;
    }

    // Vec2: Vec2{} is already (0,0); NearlyZero checks both axes (0.01px rest band).
    template<> inline Vec2 SpringValue<Vec2>::Zero() { return Vec2(0.0f, 0.0f); }
    template<> inline bool SpringValue<Vec2>::NearlyZero(const Vec2& a) {
        return std::abs(a.x) < 0.01f && std::abs(a.y) < 0.01f;
    }

} // namespace FluentUI
