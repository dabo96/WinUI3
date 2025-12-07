#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include <cmath>
#include <functional>
#include <type_traits>

namespace FluentUI {

    // Funciones de easing para animaciones suaves (Fluent Design)
    namespace Easing {
        
        // Ease Out (usado comúnmente en Fluent)
        inline float EaseOut(float t) {
            return 1.0f - std::pow(1.0f - t, 3.0f);
        }
        
        // Ease In Out (transiciones suaves)
        inline float EaseInOut(float t) {
            return t < 0.5f 
                ? 2.0f * t * t 
                : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        }
        
        // Ease In (aceleración)
        inline float EaseIn(float t) {
            return t * t * t;
        }
        
        // Linear (sin easing)
        inline float Linear(float t) {
            return t;
        }
        
        // Ease Out Cubic (estándar Fluent)
        inline float EaseOutCubic(float t) {
            return 1.0f - std::pow(1.0f - t, 3.0f);
        }
        
        // Ease Out Exponential (muy suave)
        inline float EaseOutExpo(float t) {
            return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
        }
    }

    // Clase para animar valores con transiciones suaves
    template<typename T>
    class AnimatedValue {
    public:
        AnimatedValue() : current_(T{}), target_(T{}), duration_(0.3f), elapsed_(0.0f), active_(false) {}
        
        AnimatedValue(const T& initial) 
            : current_(initial), target_(initial), duration_(0.3f), elapsed_(0.0f), active_(false) {}
        
        // Establecer valor objetivo (inicia animación)
        void SetTarget(const T& target, float duration = 0.3f, 
                      std::function<float(float)> easing = Easing::EaseOutCubic) {
            if (target_ != target) {
                start_ = current_;
                target_ = target;
                duration_ = duration;
                elapsed_ = 0.0f;
                active_ = true;
                easing_ = easing;
            }
        }
        
        // Actualizar animación (llamar cada frame)
        void Update(float deltaTime) {
            if (!active_) return;
            
            elapsed_ += deltaTime;
            float t = std::min(elapsed_ / duration_, 1.0f);
            float eased = easing_(t);
            
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
        }
        
    private:
        T Lerp(const T& a, const T& b, float t) {
            if constexpr (std::is_arithmetic_v<T>) {
                return a + (b - a) * t;
            } else {
                // Para tipos como Color, Vec2, etc., necesitarán especialización
                return a + (b - a) * t;
            }
        }
        
        T current_;
        T target_;
        T start_;
        float duration_;
        float elapsed_;
        bool active_;
        std::function<float(float)> easing_ = Easing::EaseOutCubic;
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

