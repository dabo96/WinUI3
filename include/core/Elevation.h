#pragma once
#include "Math/Color.h"
#include <algorithm>

namespace FluentUI {

    // Sistema de elevation para sombras suaves (Fluent Design)
    namespace Elevation {

        // ── Sistema z unificado ────────────────────────────────────────────
        // La "posición z" (profundidad) de una superficie determina su sombra:
        // a mayor z, sombra más amplia, desplazada y opaca. Es la ÚNICA fuente
        // de verdad para las sombras de los elementos que "flotan". Los
        // controles inline (inputs, checkbox, slider, labels…) son planos (z=0)
        // y no proyectan sombra.
        namespace Z {
            // Elementos interactivos (la z reacciona al estado). En REPOSO el botón
            // es plano (z=0, sin sombra): así se ve igual en tema claro y oscuro
            // (una sombra negra tenue es invisible sobre fondo oscuro pero visible
            // sobre fondo claro, lo que hacía que el reposo no fuese consistente).
            // La sombra "se enciende" al interactuar: hover lo eleva y el click lo
            // hunde un poco (menos que el hover) manteniendo una sombra perceptible.
            constexpr float ButtonRest    = 0.0f; // reposo: plano, sin sombra
            constexpr float ButtonPressed = 2.0f; // click: hundido respecto al hover
            constexpr float ButtonHover   = 4.0f; // elevado al pasar el ratón
            // Superficies flotantes.
            constexpr float Card    = 4.0f;   // paneles / cards
            constexpr float Flyout  = 8.0f;   // dropdowns, combobox abierto, menús, tooltips
            constexpr float Dialog  = 16.0f;  // modales / diálogos
        }

        struct ShadowParams {
            float blur;     ///< Penumbra (px lógicos) del DrawRectShadow.
            float offsetY;  ///< Desplazamiento vertical de la sombra.
            float opacity;  ///< Opacidad pico (en el borde del elemento).
        };

        // Curva continua z → (blur, offsetY, opacity).
        //  - blur    = ANCHO de la penumbra (px). Cuanto más ancha, más se "ve" el
        //    degradado (la transición se reparte sobre más píxeles). Estrecha = se
        //    ve como una línea/banda, no como difuminado.
        //  - offsetY = desplazamiento hacia abajo (da sensación de profundidad).
        //  - opacity = opacidad PICO en el borde; el render (DrawRectShadow) la
        //    difumina de ese pico hasta 0 hacia afuera. Para percibir el degradado
        //    hace falta pico suficiente Y ancho suficiente.
        // Calibrado: z=4 (hover/card) → blur ~8 / off 2 / pico 0.70. Penumbra
        // compacta (tipo difuminado de lápiz pegado al elemento). La caída
        // (kFalloff>1 en DrawRectShadow) concentra lo oscuro junto al borde y deja
        // una cola tenue: con pico alto se lee como un borde casi negro difuminado.
        inline ShadowParams Params(float z) {
            if (z <= 0.0f) return {0.0f, 0.0f, 0.0f};
            float blur    = 1.5f + 1.5f * z;
            float offsetY = 0.5f * z;
            float opacity = std::min(0.70f, 0.18f + 0.13f * z);
            return {blur, offsetY, opacity};
        }

        // Niveles de elevation estándar de Fluent Design (compat. heredada)
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

