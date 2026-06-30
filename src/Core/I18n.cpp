#include "core/I18n.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// =============================================================================
// Brief 18.6 — i18n implementation. No external dependencies (no ICU): a flat
// per-locale catalog, a minimal flat-JSON parser, and manual locale-aware number
// / date / time formatting.
// =============================================================================

namespace FluentUI {

namespace {

struct I18nState {
    std::string locale = "en-US";
    // locale -> (key -> text)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> catalogs;
    std::unordered_map<std::string, LocaleFormat> formats;
    LocaleFormat defaultFormat; // en-US defaults
};

I18nState& State() {
    static I18nState s;
    return s;
}

// --- Minimal flat-JSON parser: { "key": "value", ... } -----------------------
// Supports double-quoted strings with \" \\ \n \t \/ \uXXXX (BMP) escapes.
// Ignores anything that is not a top-level string:string pair (e.g. nested
// objects are skipped). Good enough for translation catalogs.
bool ParseString(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') return true;
        if (c == '\\' && i < s.size()) {
            char e = s[i++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'u': {
                    if (i + 4 <= s.size()) {
                        unsigned code = static_cast<unsigned>(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
                        i += 4;
                        // Encode BMP code point as UTF-8.
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                    }
                    break;
                }
                default: out += e; break;
            }
        } else {
            out += c;
        }
    }
    return false; // unterminated
}

void SkipWs(const std::string& s, size_t& i) {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        ++i;
}

bool ParseFlatJson(const std::string& json,
                   std::unordered_map<std::string, std::string>& out) {
    size_t i = 0;
    SkipWs(json, i);
    if (i >= json.size() || json[i] != '{') return false;
    ++i;
    SkipWs(json, i);
    if (i < json.size() && json[i] == '}') return true; // empty object
    while (i < json.size()) {
        SkipWs(json, i);
        std::string key;
        if (!ParseString(json, i, key)) return false;
        SkipWs(json, i);
        if (i >= json.size() || json[i] != ':') return false;
        ++i;
        SkipWs(json, i);
        if (i < json.size() && json[i] == '"') {
            std::string val;
            if (!ParseString(json, i, val)) return false;
            out[key] = val;
        } else {
            // Skip non-string value (number/object/array/bool/null) up to the
            // next top-level comma or closing brace.
            int depth = 0;
            while (i < json.size()) {
                char c = json[i];
                if (c == '{' || c == '[') depth++;
                else if (c == '}' || c == ']') {
                    if (depth == 0) break;
                    depth--;
                } else if (c == ',' && depth == 0) break;
                ++i;
            }
        }
        SkipWs(json, i);
        if (i < json.size() && json[i] == ',') { ++i; continue; }
        if (i < json.size() && json[i] == '}') return true;
        if (i >= json.size()) break;
    }
    return true;
}

// Group an unsigned digit string with the locale's grouping separator.
std::string GroupDigits(const std::string& digits, const LocaleFormat& f) {
    if (f.groupSize <= 0 || f.groupSep == '\0') return digits;
    std::string out;
    int count = 0;
    for (size_t n = digits.size(); n > 0; --n) {
        out += digits[n - 1];
        if (++count % f.groupSize == 0 && n > 1) out += f.groupSep;
    }
    std::string rev(out.rbegin(), out.rend());
    return rev;
}

std::string Pad2(int v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%02d", v);
    return b;
}

} // namespace

void RegisterTranslations(const std::string& locale,
                          const std::unordered_map<std::string, std::string>& table) {
    auto& dst = State().catalogs[locale];
    for (const auto& kv : table) dst[kv.first] = kv.second;
}

bool LoadTranslationsFromString(const std::string& locale, const std::string& json) {
    std::unordered_map<std::string, std::string> table;
    if (!ParseFlatJson(json, table)) return false;
    auto& dst = State().catalogs[locale];
    for (const auto& kv : table) dst[kv.first] = kv.second;
    return true;
}

bool LoadTranslationsFromFile(const std::string& locale, const std::string& jsonPath) {
    std::ifstream in(jsonPath, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return LoadTranslationsFromString(locale, ss.str());
}

void SetLocale(const std::string& locale) { State().locale = locale; }
const std::string& GetLocale() { return State().locale; }

void RegisterLocaleFormat(const std::string& locale, const LocaleFormat& fmt) {
    State().formats[locale] = fmt;
}

const LocaleFormat& GetLocaleFormat() {
    auto& s = State();
    auto it = s.formats.find(s.locale);
    if (it != s.formats.end()) return it->second;
    return s.defaultFormat;
}

std::string Tr(const std::string& key) {
    auto& s = State();
    auto cit = s.catalogs.find(s.locale);
    if (cit != s.catalogs.end()) {
        auto kit = cit->second.find(key);
        if (kit != cit->second.end()) return kit->second;
    }
    return key; // fallback to key
}

std::string TrFormat(const std::string& key, const std::string& arg) {
    std::string text = Tr(key);
    size_t pos = text.find("{}");
    if (pos != std::string::npos) text.replace(pos, 2, arg);
    return text;
}

std::string TrPlural(const std::string& keyBase, long long count) {
    const std::string suffix = (count == 1) ? ".one" : ".other";
    auto& s = State();
    std::string fullKey = keyBase + suffix;
    std::string text = fullKey;
    auto cit = s.catalogs.find(s.locale);
    if (cit != s.catalogs.end()) {
        auto kit = cit->second.find(fullKey);
        if (kit != cit->second.end()) {
            text = kit->second;
        } else {
            auto bit = cit->second.find(keyBase);
            text = (bit != cit->second.end()) ? bit->second : keyBase;
        }
    } else {
        text = keyBase;
    }
    std::string countStr = FormatInteger(count);
    size_t pos = text.find("{}");
    if (pos != std::string::npos) text.replace(pos, 2, countStr);
    return text;
}

std::string FormatInteger(long long value) {
    const LocaleFormat& f = GetLocaleFormat();
    bool neg = value < 0;
    unsigned long long mag = neg ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                                 : static_cast<unsigned long long>(value);
    std::string digits = std::to_string(mag);
    std::string grouped = GroupDigits(digits, f);
    return neg ? ("-" + grouped) : grouped;
}

std::string FormatNumber(double value, int decimals) {
    const LocaleFormat& f = GetLocaleFormat();
    if (decimals < 0) decimals = 0;
    bool neg = std::signbit(value) && value != 0.0;
    double mag = std::fabs(value);

    // Round to the requested number of decimals.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, mag);
    std::string str = buf;

    std::string intPart = str;
    std::string fracPart;
    size_t dot = str.find('.');
    if (dot != std::string::npos) {
        intPart = str.substr(0, dot);
        fracPart = str.substr(dot + 1);
    }
    std::string out = GroupDigits(intPart, f);
    if (decimals > 0) {
        out += f.decimalSep;
        out += fracPart;
    }
    return neg ? ("-" + out) : out;
}

std::string FormatCurrency(double value, int decimals) {
    const LocaleFormat& f = GetLocaleFormat();
    std::string num = FormatNumber(value, decimals);
    if (f.currencySymbolBefore) return f.currencySymbol + num;
    return num + " " + f.currencySymbol;
}

std::string FormatDate(int year, int month, int day) {
    const LocaleFormat& f = GetLocaleFormat();
    char y[8], m[4], d[4];
    std::snprintf(y, sizeof(y), "%04d", year);
    std::snprintf(m, sizeof(m), "%02d", month);
    std::snprintf(d, sizeof(d), "%02d", day);
    std::string sep(1, f.dateSep);
    switch (f.dateOrder) {
        case LocaleFormat::DateOrder::YMD: return std::string(y) + sep + m + sep + d;
        case LocaleFormat::DateOrder::DMY: return std::string(d) + sep + m + sep + y;
        case LocaleFormat::DateOrder::MDY:
        default:                           return std::string(m) + sep + d + sep + y;
    }
}

std::string FormatTime(int hour, int minute, int second, bool h24) {
    std::string out;
    const char* suffix = "";
    int h = hour;
    if (!h24) {
        suffix = (hour >= 12) ? " PM" : " AM";
        h = hour % 12;
        if (h == 0) h = 12;
    }
    out += Pad2(h);
    out += ":";
    out += Pad2(minute);
    if (second >= 0) {
        out += ":";
        out += Pad2(second);
    }
    out += suffix;
    return out;
}

} // namespace FluentUI
