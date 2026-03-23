#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include <string>

namespace FluentUI {

    enum class FontWeight {
        Thin = 100,
        ExtraLight = 200,
        Light = 300,
        Regular = 400,
        Medium = 500,
        SemiBold = 600,
        Bold = 700,
        ExtraBold = 800,
        Black = 900
    };

    enum class TypographyStyle {
        Caption,      // 12px
        Body,         // 14px
        BodyStrong,   // 14px Bold
        Subtitle,     // 18px
        SubtitleStrong, // 18px Bold
        Title,        // 20px
        TitleLarge,   // 28px
        Display       // 42px (para encabezados grandes)
    };

    struct ColorState {
        Color normal = Color(1.0f, 1.0f, 1.0f, 1.0f);
        Color hover = normal;
        Color pressed = normal;
        Color disabled = Color(normal.r, normal.g, normal.b, normal.a * 0.4f);
    };

    struct TextStyle {
        float fontSize = 16.0f;
        float lineHeight = 0.0f;
        FontWeight weight = FontWeight::Regular;
        Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
        std::string fontName;  // Empty = default font, otherwise uses FontManager named font
    };

    struct Typography {
        TextStyle caption;
        TextStyle body;
        TextStyle bodyStrong;
        TextStyle subtitle;
        TextStyle subtitleStrong;
        TextStyle title;
        TextStyle titleLarge;
        TextStyle display;

        const TextStyle& Get(TypographyStyle style) const {
            switch (style) {
            case TypographyStyle::Caption:        return caption;
            case TypographyStyle::BodyStrong:     return bodyStrong;
            case TypographyStyle::Subtitle:       return subtitle;
            case TypographyStyle::SubtitleStrong: return subtitleStrong;
            case TypographyStyle::Title:          return title;
            case TypographyStyle::TitleLarge:     return titleLarge;
            case TypographyStyle::Display:        return display;
            case TypographyStyle::Body:
            default:                              return body;
            }
        }
        
        TextStyle& GetMutable(TypographyStyle style) {
            switch (style) {
            case TypographyStyle::Caption:        return caption;
            case TypographyStyle::BodyStrong:     return bodyStrong;
            case TypographyStyle::Subtitle:       return subtitle;
            case TypographyStyle::SubtitleStrong: return subtitleStrong;
            case TypographyStyle::Title:          return title;
            case TypographyStyle::TitleLarge:     return titleLarge;
            case TypographyStyle::Display:        return display;
            case TypographyStyle::Body:
            default:                              return body;
            }
        }
    };

    struct ButtonStyle {
        ColorState background;
        ColorState foreground;
        ColorState border;
        Vec2 padding = Vec2(16.0f, 10.0f);
        float cornerRadius = 6.0f;
        float borderWidth = 1.0f;
        float shadowOpacity = 0.25f;
        float shadowOffsetY = 2.0f;
        TextStyle text;
    };

    struct LabelStyle {
        TextStyle text;
        Color disabledColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
    };

    struct PanelStyle {
        Color background = Color(0.18f, 0.18f, 0.18f, 1.0f);
        Color headerBackground = Color(0.23f, 0.23f, 0.23f, 1.0f);
        Color borderColor = Color(0.3f, 0.3f, 0.3f, 1.0f);
        float borderWidth = 1.0f;
        float cornerRadius = 6.0f;
        float shadowOpacity = 0.35f;
        float shadowOffsetY = 4.0f;
        TextStyle headerText;
        ColorState titleButton;
        Vec2 padding = Vec2(12.0f, 12.0f);
        bool useAcrylic = false; // Efecto acrylic Fluent Design
        float acrylicOpacity = 0.85f; // Opacidad del efecto acrylic (0.0-1.0)
    };

    struct SeparatorStyle {
        Color color = Color(0.3f, 0.3f, 0.3f, 1.0f);
        float thickness = 1.0f;
        float padding = 8.0f;
    };

    struct Style {
        Color backgroundColor = Color(0.13f, 0.13f, 0.13f, 1.0f);
        float spacing = 8.0f;
        float padding = 12.0f;
        bool isDarkTheme = true; // Cached theme mode flag

        Typography typography;
        ButtonStyle button;
        LabelStyle label;
        PanelStyle panel;
        SeparatorStyle separator;

        const TextStyle& GetTextStyle(TypographyStyle type) const {
            return typography.Get(type);
        }
    };

} // namespace FluentUI
