#pragma once
#include "Math/Color.h"

namespace FluentUI {

    // Sistema de elevation para sombras suaves (Fluent Design)
    namespace Elevation {
        
        // Niveles de elevation estándar de Fluent Design
        enum Level {
            Level0 = 0,  // Sin sombra (superficie plana)
            Level1 = 1,  // 2dp elevation
            Level2 = 2,  // 4dp elevation
            Level3 = 3,  // 8dp elevation
            Level4 = 4,  // 16dp elevation
            Level5 = 5   // 24dp elevation
        };
        
        // Obtener opacidad de sombra según el nivel
        inline float GetShadowOpacity(Level level) {
            switch (level) {
                case Level0: return 0.0f;
                case Level1: return 0.12f;
                case Level2: return 0.16f;
                case Level3: return 0.20f;
                case Level4: return 0.24f;
                case Level5: return 0.28f;
                default: return 0.0f;
            }
        }
        
        // Obtener offset Y de sombra según el nivel
        inline float GetShadowOffsetY(Level level) {
            switch (level) {
                case Level0: return 0.0f;
                case Level1: return 1.0f;
                case Level2: return 2.0f;
                case Level3: return 4.0f;
                case Level4: return 8.0f;
                case Level5: return 12.0f;
                default: return 0.0f;
            }
        }
        
        // Obtener blur radius de sombra según el nivel
        inline float GetShadowBlur(Level level) {
            switch (level) {
                case Level0: return 0.0f;
                case Level1: return 2.0f;
                case Level2: return 4.0f;
                case Level3: return 8.0f;
                case Level4: return 16.0f;
                case Level5: return 24.0f;
                default: return 0.0f;
            }
        }
        
        // Color de sombra estándar (negro con opacidad)
        inline Color GetShadowColor(Level level) {
            float opacity = GetShadowOpacity(level);
            return Color(0.0f, 0.0f, 0.0f, opacity);
        }
    }

} // namespace FluentUI

