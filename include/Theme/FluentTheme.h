#pragma once
#include "Math/Color.h"
#include "Style.h"
#include <algorithm>

namespace FluentUI {

    // Colores oficiales del Fluent Design System
    namespace FluentColors {
        // Colores base (Light theme)
        inline const Color Background = Color(1.0f, 1.0f, 1.0f, 1.0f);
        inline const Color Surface = Color(0.95f, 0.95f, 0.95f, 1.0f);
        inline const Color SurfaceAlt = Color(0.90f, 0.90f, 0.90f, 1.0f);
        inline const Color SurfaceElevated = Color(0.98f, 0.98f, 0.98f, 1.0f);

        // Colores base (Dark theme)
        inline const Color BackgroundDark = Color(0.13f, 0.13f, 0.13f, 1.0f);
        inline const Color SurfaceDark = Color(0.18f, 0.18f, 0.18f, 1.0f);
        inline const Color SurfaceAltDark = Color(0.23f, 0.23f, 0.23f, 1.0f);
        inline const Color SurfaceElevatedDark = Color(0.26f, 0.26f, 0.26f, 1.0f);

        // Colores de acento predefinidos
        inline const Color AccentBlue = Color(0.0f, 0.47f, 0.84f, 1.0f);
        inline const Color AccentGreen = Color(0.16f, 0.69f, 0.32f, 1.0f);
        inline const Color AccentPurple = Color(0.70f, 0.40f, 0.90f, 1.0f);
        inline const Color AccentOrange = Color(1.0f, 0.58f, 0.0f, 1.0f);
        inline const Color AccentPink = Color(0.94f, 0.20f, 0.55f, 1.0f);
        inline const Color AccentTeal = Color(0.0f, 0.68f, 0.70f, 1.0f);

        // Colores de acento (Fluent Blue por defecto)
        inline const Color Accent = AccentBlue;
        inline const Color AccentHover = Color(0.0f, 0.55f, 0.92f, 1.0f);
        inline const Color AccentPressed = Color(0.0f, 0.38f, 0.70f, 1.0f);

        // Colores de texto
        inline const Color TextPrimary = Color(0.13f, 0.13f, 0.13f, 1.0f);
        inline const Color TextSecondary = Color(0.6f, 0.6f, 0.6f, 1.0f);
        inline const Color TextTertiary = Color(0.8f, 0.8f, 0.8f, 1.0f);
        inline const Color TextPrimaryDark = Color(1.0f, 1.0f, 1.0f, 1.0f);
        inline const Color TextSecondaryDark = Color(0.8f, 0.8f, 0.8f, 1.0f);
        inline const Color TextTertiaryDark = Color(0.6f, 0.6f, 0.6f, 1.0f);

        // Borders
        inline const Color Border = Color(0.8f, 0.8f, 0.8f, 1.0f);
        inline const Color BorderDark = Color(0.3f, 0.3f, 0.3f, 1.0f);
        inline const Color BorderLight = Color(0.9f, 0.9f, 0.9f, 1.0f);
        
        // Container-specific borders - más visibles
        inline const Color ContainerBorderDark = Color(0.4f, 0.4f, 0.4f, 1.0f);  // Borde más visible en tema oscuro
        inline const Color ContainerBorderLight = Color(0.7f, 0.7f, 0.7f, 1.0f); // Borde para tema claro
        
        // Container backgrounds - más distintivos
        inline const Color ContainerBackgroundDark = Color(0.16f, 0.16f, 0.16f, 1.0f);  // Fondo ligeramente más claro que background
        inline const Color ContainerBackgroundLight = Color(0.96f, 0.96f, 0.96f, 1.0f); // Fondo ligeramente más oscuro que background

        // Estados
        inline const Color Disabled = Color(0.7f, 0.7f, 0.7f, 1.0f);
        inline const Color Error = Color(0.95f, 0.26f, 0.21f, 1.0f);
        inline const Color Success = Color(0.16f, 0.69f, 0.32f, 1.0f);
        inline const Color Warning = Color(1.0f, 0.58f, 0.0f, 1.0f);
        inline const Color Info = Color(0.0f, 0.47f, 0.84f, 1.0f);

        // Helper para generar colores de acento con variaciones
        inline Color GetAccentHover(const Color& accent) {
            return Color(std::min(1.0f, accent.r * 1.15f), 
                        std::min(1.0f, accent.g * 1.15f), 
                        std::min(1.0f, accent.b * 1.15f), accent.a);
        }

        inline Color GetAccentPressed(const Color& accent) {
            return Color(accent.r * 0.85f, accent.g * 0.85f, accent.b * 0.85f, accent.a);
        }
    } // namespace FluentColors

    // Tema Fluent predeterminado
    Style GetDefaultFluentStyle();
    Style GetDarkFluentStyle();
    
    // Crear tema personalizado con color de acento
    Style CreateCustomFluentStyle(const Color& accentColor, bool darkTheme = true);

} // namespace FluentUI
