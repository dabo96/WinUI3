#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace FluentUI {

    struct Ripple {
        Vec2 center;
        float radius = 0.0f;
        float maxRadius = 0.0f;
        float opacity = 1.0f;
        float duration = 0.4f;
        float elapsed = 0.0f;
        bool active = false;
    };

    class RippleEffect {
    public:
        // Agregar un nuevo ripple en la posición especificada
        void AddRipple(const Vec2& position, float maxRadius = 100.0f, float duration = 0.4f) {
            Ripple ripple;
            ripple.center = position;
            ripple.maxRadius = maxRadius;
            ripple.duration = duration;
            ripple.active = true;
            ripples_.push_back(ripple);
        }

        // Actualizar todos los ripples
        void Update(float deltaTime) {
            for (auto& ripple : ripples_) {
                if (!ripple.active) continue;
                
                ripple.elapsed += deltaTime;
                float t = ripple.elapsed / ripple.duration;
                
                if (t >= 1.0f) {
                    ripple.active = false;
                    continue;
                }
                
                // Easing out para el radio
                float easedT = 1.0f - std::pow(1.0f - t, 3.0f);
                ripple.radius = ripple.maxRadius * easedT;
                
                // Fade out para la opacidad
                ripple.opacity = 1.0f - t;
            }
            
            // Eliminar ripples inactivos
            ripples_.erase(
                std::remove_if(ripples_.begin(), ripples_.end(),
                    [](const Ripple& r) { return !r.active; }),
                ripples_.end()
            );
        }

        // Obtener todos los ripples activos
        const std::vector<Ripple>& GetRipples() const { return ripples_; }

        // Check if any ripples are active
        bool IsActive() const { return !ripples_.empty(); }

        // Limpiar todos los ripples
        void Clear() { ripples_.clear(); }

    private:
        std::vector<Ripple> ripples_;
    };

} // namespace FluentUI

