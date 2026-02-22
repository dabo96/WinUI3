#include "Theme/FluentTheme.h"

namespace FluentUI {

    namespace {
        TextStyle MakeTextStyle(float size, FontWeight weight, const Color& color, float lineHeight = 0.0f)
        {
            TextStyle ts;
            ts.fontSize = size;
            ts.weight = weight;
            ts.color = color;
            ts.lineHeight = lineHeight;
            return ts;
        }

        ButtonStyle MakeButtonStyle(const Color& base, const Color& hover, const Color& pressed, const Color& disabledForeground, const Color& textColor)
        {
            ButtonStyle style;
            style.background.normal = base;
            style.background.hover = hover;
            style.background.pressed = pressed;
            style.background.disabled = Color(base.r, base.g, base.b, 0.4f);

            style.foreground.normal = textColor;
            style.foreground.hover = textColor;
            style.foreground.pressed = textColor;
            style.foreground.disabled = disabledForeground;

            style.border.normal = Color(base.r, base.g, base.b, 0.0f);
            style.border.hover = Color(base.r, base.g, base.b, 0.0f);
            style.border.pressed = Color(base.r, base.g, base.b, 0.0f);
            style.border.disabled = Color(0.0f, 0.0f, 0.0f, 0.0f);

            style.cornerRadius = 6.0f;
            style.padding = Vec2(20.0f, 10.0f);
            style.borderWidth = 0.0f;
            style.shadowOpacity = 0.25f;
            style.shadowOffsetY = 2.0f;
            style.text = MakeTextStyle(16.0f, FontWeight::SemiBold, textColor);
            return style;
        }

        PanelStyle MakePanelStyle(bool darkTheme)
        {
            PanelStyle panel;
            if (darkTheme)
            {
                // Fondo más claro que el background para mejor contraste (como Windows Settings)
                panel.background = FluentColors::ContainerBackgroundDark;
                // Hacer el fondo aún más distintivo
                panel.background = Color(
                    panel.background.r * 1.12f,
                    panel.background.g * 1.12f,
                    panel.background.b * 1.12f,
                    1.0f
                );
                panel.headerBackground = FluentColors::SurfaceAltDark;
                panel.borderColor = FluentColors::ContainerBorderDark;  // Para uso futuro si se necesita
                panel.headerText = MakeTextStyle(16.0f, FontWeight::SemiBold, FluentColors::TextPrimaryDark);
                panel.titleButton.normal = FluentColors::AccentHover;
                panel.titleButton.hover = FluentColors::Accent;
                panel.titleButton.pressed = FluentColors::AccentPressed;
                panel.titleButton.disabled = Color(FluentColors::AccentHover.r, FluentColors::AccentHover.g, FluentColors::AccentHover.b, 0.4f);
            }
            else
            {
                // Fondo más oscuro que el background para mejor contraste
                panel.background = FluentColors::ContainerBackgroundLight;
                // Hacer el fondo aún más distintivo
                panel.background = Color(
                    panel.background.r * 0.94f,
                    panel.background.g * 0.94f,
                    panel.background.b * 0.94f,
                    1.0f
                );
                panel.headerBackground = FluentColors::SurfaceAlt;
                panel.borderColor = FluentColors::ContainerBorderLight;  // Para uso futuro si se necesita
                panel.headerText = MakeTextStyle(16.0f, FontWeight::SemiBold, FluentColors::TextPrimary);
                panel.titleButton.normal = FluentColors::Accent;
                panel.titleButton.hover = FluentColors::AccentHover;
                panel.titleButton.pressed = FluentColors::AccentPressed;
                panel.titleButton.disabled = Color(FluentColors::Accent.r, FluentColors::Accent.g, FluentColors::Accent.b, 0.4f);
            }

            panel.borderWidth = 0.0f;  // Sin borde visible - solo contraste de fondo
            panel.cornerRadius = 10.0f;
            panel.shadowOpacity = 0.4f;
            panel.shadowOffsetY = 8.0f;
            panel.padding = Vec2(16.0f, 14.0f);
            panel.useAcrylic = true; // Habilitar acrylic por defecto en paneles
            panel.acrylicOpacity = 0.85f;
            return panel;
        }

        SeparatorStyle MakeSeparatorStyle(bool darkTheme)
        {
            SeparatorStyle separator;
            separator.color = darkTheme ? Color(1.0f, 1.0f, 1.0f, 0.15f)
                                        : Color(0.0f, 0.0f, 0.0f, 0.15f);
            separator.thickness = 1.0f;
            separator.padding = 12.0f;
            return separator;
        }

        LabelStyle MakeLabelStyle(const TextStyle& baseText)
        {
            LabelStyle label;
            label.text = baseText;
            label.disabledColor = Color(baseText.color.r, baseText.color.g, baseText.color.b, baseText.color.a * 0.45f);
            return label;
        }

        Style BuildStyle(bool darkTheme)
        {
            Style style;
            style.isDarkTheme = darkTheme;
            style.backgroundColor = darkTheme ? FluentColors::BackgroundDark : FluentColors::Background;
            style.spacing = 10.0f;
            style.padding = 14.0f;

            const Color textPrimary = darkTheme ? FluentColors::TextPrimaryDark : FluentColors::TextPrimary;
            const Color textSecondary = darkTheme ? FluentColors::TextSecondaryDark : FluentColors::TextSecondary;
            const Color textTertiary = darkTheme ? FluentColors::TextTertiaryDark : FluentColors::TextTertiary;

            // Tamaños de fuente según Fluent Design System
            style.typography.caption = MakeTextStyle(13.0f, FontWeight::Regular, textSecondary, 18.0f);
            style.typography.body = MakeTextStyle(14.0f, FontWeight::Regular, textPrimary, 20.0f);
            style.typography.bodyStrong = MakeTextStyle(14.0f, FontWeight::SemiBold, textPrimary, 20.0f);
            style.typography.subtitle = MakeTextStyle(18.0f, FontWeight::Regular, textPrimary, 24.0f);
            style.typography.subtitleStrong = MakeTextStyle(18.0f, FontWeight::SemiBold, textPrimary, 24.0f);
            style.typography.title = MakeTextStyle(20.0f, FontWeight::SemiBold, textPrimary, 28.0f);
            style.typography.titleLarge = MakeTextStyle(28.0f, FontWeight::SemiBold, textPrimary, 36.0f);
            style.typography.display = MakeTextStyle(42.0f, FontWeight::Bold, textPrimary, 52.0f);

            // Botón con mejor contraste y sombras más suaves
            ButtonStyle buttonStyle = MakeButtonStyle(
                FluentColors::Accent,
                FluentColors::AccentHover,
                FluentColors::AccentPressed,
                Color(textPrimary.r, textPrimary.g, textPrimary.b, 0.45f),
                Color(1.0f, 1.0f, 1.0f, 1.0f));
            style.button = buttonStyle;

            style.label = MakeLabelStyle(style.typography.body);
            style.panel = MakePanelStyle(darkTheme);
            style.separator = MakeSeparatorStyle(darkTheme);

            return style;
        }
    } // namespace

    Style GetDefaultFluentStyle() {
        return BuildStyle(false);
    }

    Style GetDarkFluentStyle() {
        return BuildStyle(true);
    }

    Style CreateCustomFluentStyle(const Color& accentColor, bool darkTheme) {
        Style style = BuildStyle(darkTheme);

        // Aplicar color de acento personalizado
        const Color accentHover = FluentColors::GetAccentHover(accentColor);
        const Color accentPressed = FluentColors::GetAccentPressed(accentColor);
        // Choose white or black text based on accent luminance
        float luminance = 0.2126f * accentColor.r + 0.7152f * accentColor.g + 0.0722f * accentColor.b;
        const Color textColor = luminance > 0.5f
            ? Color(0.0f, 0.0f, 0.0f, 1.0f)   // Dark text on light accent
            : Color(1.0f, 1.0f, 1.0f, 1.0f);   // White text on dark accent
        
        ButtonStyle buttonStyle = MakeButtonStyle(
            accentColor,
            accentHover,
            accentPressed,
            Color(textColor.r, textColor.g, textColor.b, 0.45f),
            textColor);
        buttonStyle.shadowOpacity = 0.25f;
        buttonStyle.shadowOffsetY = 2.0f;
        buttonStyle.cornerRadius = 6.0f;
        style.button = buttonStyle;

        // Aplicar acento a otros elementos si es necesario
        style.panel.titleButton.normal = accentColor;
        style.panel.titleButton.hover = accentHover;
        style.panel.titleButton.pressed = accentPressed;
        
        return style;
    }

} // namespace FluentUI

