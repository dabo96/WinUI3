#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include <cmath>
#include <type_traits>

namespace FluentUI {

    // Perf 2.1: Easing type enum (1 byte) replaces std::function (32+ bytes)
    enum class EasingType : uint8_t {
        Linear = 0,
        EaseIn,
        EaseOut,
        EaseInOut,
        EaseOutCubic,
        EaseOutExpo
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

        // Perf 2.1: Evaluate easing by enum (no function pointer overhead)
        inline float Evaluate(EasingType type, float t) {
            switch (type) {
                case EasingType::Linear:       return t;
                case EasingType::EaseIn:       return t * t * t;
                case EasingType::EaseOut:       { float u = 1.0f - t; return 1.0f - u * u * u; }
                case EasingType::EaseInOut:    return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
                case EasingType::EaseOutCubic: { float u = 1.0f - t; return 1.0f - u * u * u; }
                case EasingType::EaseOutExpo:  return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
                default: return t;
            }
        }
    }

    // Clase para animar valores con transiciones suaves
    template<typename T>
    class AnimatedValue {
    public:
        AnimatedValue() : current_(T{}), target_(T{}), duration_(0.3f), elapsed_(0.0f), active_(false), initialized_(false) {}

        AnimatedValue(const T& initial)
            : current_(initial), target_(initial), duration_(0.3f), elapsed_(0.0f), active_(false), initialized_(true) {}

        // Perf 2.1: SetTarget with EasingType enum (backward compatible overload below)
        void SetTarget(const T& target, float duration, EasingType easing) {
            if (target_ != target) {
                start_ = current_;
                target_ = target;
                duration_ = duration;
                elapsed_ = 0.0f;
                active_ = true;
                easingType_ = easing;
            }
        }

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

} // namespace FluentUI
