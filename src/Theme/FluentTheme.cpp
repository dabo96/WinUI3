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
            // Sombra ambiental difusa: opacidad baja + offset pequeño + blur
            // amplio. Un pico alto concentraba el negro en el borde inferior y se
            // leía como una banda dura; bajarlo lo convierte en un halo sutil.
            panel.shadowOpacity = 0.22f;
            panel.shadowOffsetY = 2.0f;
            panel.shadowBlur = 10.0f;
            panel.padding = Vec2(16.0f, 14.0f);
            panel.useAcrylic = false; // Deshabilitar acrylic por defecto para evitar transparencias no deseadas
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
        // Botones casi planos (estilo Fluent): apenas un velo de sombra para dar
        // un mínimo de elevación, sin la banda oscura inferior que se veía pesada.
        buttonStyle.shadowOpacity = 0.10f;
        buttonStyle.shadowOffsetY = 1.0f;
        buttonStyle.shadowBlur = 6.0f;
        buttonStyle.cornerRadius = 6.0f;
        style.button = buttonStyle;

        // Brand accent usado por sliders, checkboxes, progress bars, radios,
        // plots, date pickers, etc. (ctx->style.accentColor). Sin esto se queda
        // en el azul por defecto y el acento personalizado no se reflejaría en
        // todos esos widgets aunque el botón sí cambie (usa button.background).
        style.accentColor = accentColor;

        // Aplicar acento a otros elementos si es necesario
        style.panel.titleButton.normal = accentColor;
        style.panel.titleButton.hover = accentHover;
        style.panel.titleButton.pressed = accentPressed;
        
        return style;
    }

    // Phase 6: High Contrast Accessibility Theme
    Style GetHighContrastStyle() {
        Style style;
        style.isDarkTheme = true;

        // Pure black background, pure white text
        Color black(0.0f, 0.0f, 0.0f, 1.0f);
        Color white(1.0f, 1.0f, 1.0f, 1.0f);
        Color yellow(1.0f, 1.0f, 0.0f, 1.0f);   // Highlight/accent
        Color cyan(0.0f, 1.0f, 1.0f, 1.0f);      // Links/interactive
        Color green(0.0f, 1.0f, 0.0f, 1.0f);      // Enabled states
        Color gray(0.5f, 0.5f, 0.5f, 1.0f);       // Disabled

        style.backgroundColor = black;
        style.spacing = 8.0f;
        style.padding = 12.0f;

        // Typography — large, high-contrast text
        auto makeText = [&](float size) -> TextStyle {
            return {size, 0.0f, FontWeight::Regular, white};
        };
        style.typography.caption = makeText(13.0f);
        style.typography.body = makeText(15.0f);
        style.typography.bodyStrong = {15.0f, 0.0f, FontWeight::Bold, white};
        style.typography.subtitle = makeText(20.0f);
        style.typography.subtitleStrong = {20.0f, 0.0f, FontWeight::Bold, white};
        style.typography.title = makeText(24.0f);
        style.typography.titleLarge = makeText(32.0f);
        style.typography.display = makeText(48.0f);

        // Button — bright borders, clear states
        style.button.background = {black, yellow, cyan, Color(0.2f, 0.2f, 0.2f, 1.0f)};
        style.button.foreground = {white, black, black, gray};
        style.button.border = {white, yellow, cyan, gray};
        style.button.padding = Vec2(18.0f, 12.0f);
        style.button.cornerRadius = 4.0f;
        style.button.borderWidth = 2.0f;
        style.button.shadowOpacity = 0.0f;
        style.button.text = {15.0f, 0.0f, FontWeight::Bold, white};

        // Label
        style.label.text = {15.0f, 0.0f, FontWeight::Regular, white};
        style.label.disabledColor = gray;

        // Panel — clear borders
        style.panel.background = Color(0.05f, 0.05f, 0.05f, 1.0f);
        style.panel.headerBackground = Color(0.1f, 0.1f, 0.1f, 1.0f);
        style.panel.borderColor = white;
        style.panel.borderWidth = 2.0f;
        style.panel.cornerRadius = 4.0f;
        style.panel.shadowOpacity = 0.0f;
        style.panel.headerText = {15.0f, 0.0f, FontWeight::Bold, white};
        style.panel.titleButton = {cyan, yellow, green, gray};
        style.panel.padding = Vec2(12.0f, 12.0f);
        style.panel.useAcrylic = false; // No acrylic in high contrast

        // Separator
        style.separator.color = white;
        style.separator.thickness = 2.0f;
        style.separator.padding = 8.0f;

        return style;
    }

    Style GetEditorDarkStyle() {
        Style style;
        style.isDarkTheme = true;

        // Increased contrast dark theme — clear visual hierarchy:
        // menubar (darkest) → toolbar → viewport bg → panels (lightest dark)
        Color bg0 = Color::FromHex("#141414");   // viewport/menubar — nearly black
        Color bg1 = Color::FromHex("#1e1e1e");   // toolbar
        Color bg2 = Color::FromHex("#252525");   // panels
        Color bg3 = Color::FromHex("#2e2e2e");   // panel headers, button bg
        Color borderColor = Color::FromHex("#3a3a3a");  // more visible borders
        Color borderSoft = Color::FromHex("#333333");
        Color textMain = Color::FromHex("#e8e8e8");  // slightly brighter
        Color textDim = Color::FromHex("#9a9a9a");
        Color textMuted = Color::FromHex("#6b6b6b");
        Color accent = Color::FromHex("#3b82f6");
        Color accentHover = Color::FromHex("#2563eb");
        Color accentPressed = Color::FromHex("#1d4ed8");

        style.backgroundColor = bg0;
        style.spacing = 10.0f;
        style.padding = 14.0f;
        style.accentColor = accent;
        // Alpha=0 → slider falls back to accentColor (Fluent 2 brand fill)
        style.sliderFillColor = Color(0.0f, 0.0f, 0.0f, 0.0f);

        // Typography aligned to Fluent 2 fontSizeBase tokens:
        //   100=10, 200=12 (caption), 300=14 (body), 400=16, 500=20, 600=24
        style.typography.caption    = MakeTextStyle(12.0f, FontWeight::Regular, textDim, 18.0f);
        style.typography.body       = MakeTextStyle(14.0f, FontWeight::Regular, textMain, 20.0f);
        style.typography.bodyStrong = MakeTextStyle(14.0f, FontWeight::SemiBold, textMain, 20.0f);
        style.typography.subtitle   = MakeTextStyle(16.0f, FontWeight::Regular, textMain, 22.0f);
        style.typography.subtitleStrong = MakeTextStyle(16.0f, FontWeight::SemiBold, textMain, 22.0f);
        style.typography.title      = MakeTextStyle(20.0f, FontWeight::SemiBold, textMain, 28.0f);
        style.typography.titleLarge = MakeTextStyle(24.0f, FontWeight::SemiBold, textMain, 32.0f);
        style.typography.display    = MakeTextStyle(36.0f, FontWeight::Bold, textMain, 46.0f);

        // Buttons — subtle, blending with panels; accent for active/CTA
        ButtonStyle btn;
        btn.background.normal  = bg3;
        btn.background.hover   = borderColor;
        btn.background.pressed = bg2;
        btn.background.disabled = Color(bg3.r, bg3.g, bg3.b, 0.4f);
        btn.foreground.normal  = textMain;
        btn.foreground.hover   = textMain;
        btn.foreground.pressed = textMain;
        btn.foreground.disabled = textMuted;
        btn.border.normal  = borderSoft;
        btn.border.hover   = borderColor;
        btn.border.pressed = borderColor;
        btn.border.disabled = Color(0, 0, 0, 0);
        btn.cornerRadius = 4.0f;                    // borderRadiusMedium
        btn.padding = Vec2(12.0f, 5.0f);            // spacingHorizontalM × Fluent Medium V padding
        btn.borderWidth = 1.0f;                     // strokeThin
        btn.shadowOpacity = 0.0f;
        btn.shadowOffsetY = 0.0f;
        btn.text = MakeTextStyle(14.0f, FontWeight::Regular, textMain);  // fontSizeBase300
        style.button = btn;

        // Labels
        style.label.text = MakeTextStyle(14.0f, FontWeight::Regular, textMain);
        style.label.disabledColor = Color(textMain.r, textMain.g, textMain.b, 0.45f);

        // Panels — flat, subtle borders, no heavy shadows
        // Panels are the LIGHTEST dark element to contrast with the dark viewport
        PanelStyle panel;
        panel.background = bg2;
        panel.headerBackground = bg3;
        panel.borderColor = borderSoft;
        panel.borderWidth = 1.0f;
        panel.cornerRadius = 0.0f;   // Flat, docked panels (no rounded corners)
        panel.shadowOpacity = 0.0f;
        panel.shadowOffsetY = 0.0f;
        panel.padding = Vec2(14.0f, 12.0f);
        panel.headerText = MakeTextStyle(12.0f, FontWeight::SemiBold, textMain);
        panel.titleButton.normal = accent;
        panel.titleButton.hover = accentHover;
        panel.titleButton.pressed = accentPressed;
        panel.titleButton.disabled = Color(accent.r, accent.g, accent.b, 0.4f);
        panel.useAcrylic = false;
        panel.acrylicOpacity = 1.0f;
        style.panel = panel;

        // Separator
        style.separator.color = borderSoft;
        style.separator.thickness = 1.0f;
        style.separator.padding = 8.0f;

        return style;
    }

} // namespace FluentUI

