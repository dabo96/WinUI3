#pragma once
// =============================================================================
// Brief 18.6 — i18n / localization
// -----------------------------------------------------------------------------
// A tiny, dependency-free localization layer:
//   * A string catalog (key -> translated text) with per-locale tables and a
//     fallback to the key itself when a translation is missing.
//   * Locale-aware number / date / time / currency formatting helpers built on
//     std::locale-independent manual formatting (no ICU dependency), driven by a
//     small LocaleFormat descriptor (decimal/grouping separators, date order).
//   * Basic plural selection (one / other) good enough for English-like rules;
//     languages with richer plural rules can register a custom selector.
//
// Out of scope (documented): full CLDR plural rules, message-format argument
// substitution beyond simple "{}" placeholders, RTL bidi shaping (see brief 18.5).
// =============================================================================
#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace FluentUI {

// Separators / ordering that vary by locale. Defaults match en-US.
struct LocaleFormat {
    char decimalSep = '.';       // 1.5
    char groupSep = ',';         // 1,000,000
    int groupSize = 3;           // digits per group
    // Date ordering: how Y/M/D are arranged. 0=Y-M-D, 1=M-D-Y, 2=D-M-Y.
    enum class DateOrder { YMD, MDY, DMY } dateOrder = DateOrder::MDY;
    char dateSep = '/';          // 12/31/2026
    std::string currencySymbol = "$";
    bool currencySymbolBefore = true; // "$1.00" vs "1.00 kr"
};

// =============================================================================
// Catalog / translation
// =============================================================================

// Load a translation table for a locale from an in-memory map (key -> text).
void RegisterTranslations(const std::string& locale,
                          const std::unordered_map<std::string, std::string>& table);

// Load a translation table for a locale from a flat-JSON file:
//   { "key.one": "Texto", "key.two": "Otro" }
// Returns false if the file cannot be opened or parsed. Existing entries for the
// locale are merged (new keys override).
bool LoadTranslationsFromFile(const std::string& locale, const std::string& jsonPath);

// Parse a flat-JSON string ({"k":"v", ...}) into the given locale's table.
bool LoadTranslationsFromString(const std::string& locale, const std::string& json);

// Set / get the active locale. Unknown locales still work (everything falls back
// to the key text and en-US number formats unless a LocaleFormat is registered).
void SetLocale(const std::string& locale);
const std::string& GetLocale();

// Register the numeric/date format descriptor for a locale.
void RegisterLocaleFormat(const std::string& locale, const LocaleFormat& fmt);
// Returns the format for the active locale (or an en-US default).
const LocaleFormat& GetLocaleFormat();

// Translate a key using the active locale; falls back to the key itself when the
// key is missing (so untranslated UI still shows something meaningful).
std::string Tr(const std::string& key);

// Translate + substitute the first "{}" placeholder with `arg` (simple, one-arg).
std::string TrFormat(const std::string& key, const std::string& arg);

// Plural helper: picks `keyBase + ".one"` when count==1, else `keyBase + ".other"`,
// then substitutes "{}" with the count. Falls back to keyBase if neither exists.
std::string TrPlural(const std::string& keyBase, long long count);

// =============================================================================
// Locale-aware formatting (catalog-independent; usable by NumberBox/DateTime)
// =============================================================================

// Format an integer with grouping separators per the active locale.
std::string FormatInteger(long long value);
// Format a real number with `decimals` fractional digits and grouping.
std::string FormatNumber(double value, int decimals = 2);
// Format as currency (symbol + grouped number) per the active locale.
std::string FormatCurrency(double value, int decimals = 2);
// Format a calendar date per the active locale's order/separator.
std::string FormatDate(int year, int month, int day);
// Format a clock time. `h24` selects 24-hour vs 12-hour (AM/PM) presentation.
std::string FormatTime(int hour, int minute, int second = -1, bool h24 = true);

} // namespace FluentUI
