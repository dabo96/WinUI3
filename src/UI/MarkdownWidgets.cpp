#include <SDL3/SDL.h>
// brief 17 — MarkdownView: render a read-only subset of Markdown.
//
// Supported: ATX headings (#..######), inline **bold** / *italic* / `code`,
// links [txt](url), unordered lists (- / *), ordered lists (1.), block quotes
// (>), fenced code blocks (```), images ![alt](url) and horizontal rules (---).
//
// It is NOT an editor and NOT a full CommonMark implementation (no tables,
// footnotes, nested blocks, reference links). The parser is line-oriented:
// classify each line into a block, accumulate paragraph runs, then render block
// by block. Inline emphasis is rendered segment-by-segment (pseudo-bold via a
// double draw, since no dedicated bold/italic face is guaranteed) and wrapped by
// word using GetGlyphAdvance/MeasureText — the same machinery as the rest of the
// text widgets. Links are drawn inline (accent + underline + hand cursor +
// SDL_OpenURL on click), matching HyperlinkButton's behaviour at an absolute
// position (HyperlinkButton itself is a flow widget and can't be placed mid-line).
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "UI/Icons.h"
#include "Theme/FluentTheme.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace FluentUI {

// ── Image registry (url -> texture handle + intrinsic size) ─────────────────
namespace {
struct MdImage {
  void *handle = nullptr;
  Vec2 size{0, 0};
};
std::unordered_map<std::string, MdImage> &MdImages() {
  static std::unordered_map<std::string, MdImage> reg;
  return reg;
}
} // namespace

void MarkdownRegisterImage(const std::string &url, void *textureHandle, Vec2 size) {
  if (textureHandle == nullptr)
    MdImages().erase(url);
  else
    MdImages()[url] = MdImage{textureHandle, size};
}

namespace {

// One styled inline run.
struct Seg {
  std::string text;
  bool bold = false;
  bool italic = false;
  bool code = false;
  bool link = false;
  std::string url;
};

// Parse inline emphasis/code/links from a single logical text run.
std::vector<Seg> ParseInline(const std::string &s) {
  std::vector<Seg> out;
  std::string cur;
  bool bold = false, ital = false;
  auto flush = [&]() {
    if (!cur.empty()) {
      Seg seg;
      seg.text = cur;
      seg.bold = bold;
      seg.italic = ital;
      out.push_back(seg);
      cur.clear();
    }
  };
  size_t i = 0, n = s.size();
  while (i < n) {
    char c = s[i];
    if (c == '`') {
      size_t j = s.find('`', i + 1);
      if (j == std::string::npos) {
        cur += s.substr(i);
        break;
      }
      flush();
      Seg seg;
      seg.text = s.substr(i + 1, j - i - 1);
      seg.code = true;
      out.push_back(seg);
      i = j + 1;
      continue;
    }
    if (c == '[') {
      size_t rb = s.find(']', i + 1);
      if (rb != std::string::npos && rb + 1 < n && s[rb + 1] == '(') {
        size_t rp = s.find(')', rb + 2);
        if (rp != std::string::npos) {
          flush();
          Seg seg;
          seg.text = s.substr(i + 1, rb - i - 1);
          seg.link = true;
          seg.url = s.substr(rb + 2, rp - rb - 2);
          out.push_back(seg);
          i = rp + 1;
          continue;
        }
      }
    }
    if (c == '*') {
      if (i + 1 < n && s[i + 1] == '*') {
        flush();
        bold = !bold;
        i += 2;
        continue;
      }
      flush();
      ital = !ital;
      ++i;
      continue;
    }
    cur += c;
    ++i;
  }
  flush();
  return out;
}

struct Tok {
  std::string text;
  bool isSpace = false;
  bool bold = false, italic = false, code = false, link = false;
  std::string url;
  float w = 0.0f;
};

// Lay out + draw a sequence of inline segments starting at `start`, wrapping by
// word inside `maxW`. Returns the total height consumed. `forceBold` bolds every
// token (headings); `baseColor` is the default text color.
float RenderInline(UIContext *ctx, const std::vector<Seg> &segs, Vec2 start,
                   float maxW, float fs, bool forceBold, const Color &baseColor) {
  std::vector<Tok> toks;
  for (const auto &sg : segs) {
    size_t i = 0, n = sg.text.size();
    while (i < n) {
      if (sg.text[i] == ' ' || sg.text[i] == '\t') {
        Tok t;
        t.text = " ";
        t.isSpace = true;
        t.bold = sg.bold || forceBold;
        t.italic = sg.italic;
        t.code = sg.code;
        t.link = sg.link;
        t.url = sg.url;
        t.w = MeasureTextCached(ctx, " ", fs).x;
        toks.push_back(t);
        ++i;
        while (i < n && (sg.text[i] == ' ' || sg.text[i] == '\t'))
          ++i;
        continue;
      }
      size_t start_i = i;
      while (i < n && sg.text[i] != ' ' && sg.text[i] != '\t')
        ++i;
      Tok t;
      t.text = sg.text.substr(start_i, i - start_i);
      t.bold = sg.bold || forceBold;
      t.italic = sg.italic;
      t.code = sg.code;
      t.link = sg.link;
      t.url = sg.url;
      t.w = MeasureTextCached(ctx, t.text, fs).x;
      toks.push_back(t);
    }
  }

  float lineH = fs * 1.45f;
  float x = start.x;
  float y = start.y;
  float lineStartX = start.x;
  bool lineEmpty = true;

  auto drawTok = [&](const Tok &t, float tx) {
    Color col = baseColor;
    if (t.link)
      col = FluentColors::Accent;
    if (t.code) {
      // subtle code background
      Color bg(0.5f, 0.5f, 0.5f, 0.18f);
      ctx->renderer.DrawRectFilled(Vec2(tx - 1.0f, y + 1.0f),
                                   Vec2(t.w + 2.0f, lineH - 2.0f), bg, 2.0f);
      col = Color(0.95f, 0.6f, 0.4f, 1.0f);
    }
    Vec2 tp(tx, y + (lineH - fs) * 0.5f);
    ctx->renderer.DrawText(tp, t.text, col, fs);
    if (t.bold) // pseudo-bold: redraw with a 0.5px offset
      ctx->renderer.DrawText(Vec2(tp.x + 0.5f, tp.y), t.text, col, fs);
    if (t.link) {
      float uy = tp.y + fs * 0.95f;
      ctx->renderer.DrawLine(Vec2(tx, uy), Vec2(tx + t.w, uy), col, 1.0f);
      Vec2 lpos(tx, tp.y);
      Vec2 lsize(t.w, fs);
      if (IsMouseOver(ctx, lpos, lsize)) {
        ctx->desiredCursor = UIContext::CursorType::Hand;
        if (ctx->input.IsMousePressed(0) && !t.url.empty())
          SDL_OpenURL(t.url.c_str());
      }
    }
  };

  for (size_t k = 0; k < toks.size(); ++k) {
    const Tok &t = toks[k];
    if (!t.isSpace && !lineEmpty && (x + t.w) > lineStartX + maxW) {
      // wrap
      x = lineStartX;
      y += lineH;
      lineEmpty = true;
    }
    if (t.isSpace && lineEmpty)
      continue; // fold leading spaces
    drawTok(t, x);
    x += t.w;
    lineEmpty = false;
  }
  return (y - start.y) + lineH;
}

float MeasureInlineHeight(UIContext *ctx, const std::vector<Seg> &segs, float maxW,
                          float fs) {
  // Mirror RenderInline's wrapping without drawing.
  std::vector<float> wordW;
  std::vector<bool> isSpace;
  for (const auto &sg : segs) {
    size_t i = 0, n = sg.text.size();
    while (i < n) {
      if (sg.text[i] == ' ' || sg.text[i] == '\t') {
        wordW.push_back(MeasureTextCached(ctx, " ", fs).x);
        isSpace.push_back(true);
        ++i;
        while (i < n && (sg.text[i] == ' ' || sg.text[i] == '\t'))
          ++i;
        continue;
      }
      size_t s_i = i;
      while (i < n && sg.text[i] != ' ' && sg.text[i] != '\t')
        ++i;
      wordW.push_back(MeasureTextCached(ctx, sg.text.substr(s_i, i - s_i), fs).x);
      isSpace.push_back(false);
    }
  }
  float lineH = fs * 1.45f;
  float x = 0.0f;
  int lines = 1;
  bool lineEmpty = true;
  for (size_t k = 0; k < wordW.size(); ++k) {
    if (!isSpace[k] && !lineEmpty && x + wordW[k] > maxW) {
      x = 0.0f;
      lines++;
      lineEmpty = true;
    }
    if (isSpace[k] && lineEmpty)
      continue;
    x += wordW[k];
    lineEmpty = false;
  }
  return static_cast<float>(lines) * lineH;
}

// Trim trailing CR and leading spaces helpers.
std::string RStripCR(const std::string &s) {
  if (!s.empty() && s.back() == '\r')
    return s.substr(0, s.size() - 1);
  return s;
}

bool IsHRule(const std::string &s) {
  std::string t;
  for (char c : s)
    if (c != ' ')
      t += c;
  if (t.size() < 3)
    return false;
  char c0 = t[0];
  if (c0 != '-' && c0 != '*' && c0 != '_')
    return false;
  for (char c : t)
    if (c != c0)
      return false;
  return true;
}

} // namespace

void MarkdownView(const std::string &id, const std::string &markdown,
                  float maxWidth) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  const TextStyle &body = ctx->style.GetTextStyle(TypographyStyle::Body);
  float baseFs = body.fontSize;
  Color baseColor = body.color;

  float maxW = maxWidth;
  if (maxW <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    maxW = avail.x;
    if (maxW <= 0.0f)
      maxW = ctx->renderer.GetViewportSize().x - ctx->cursorPos.x;
  }

  // Split into lines.
  std::vector<std::string> rawLines;
  {
    size_t start = 0;
    for (size_t i = 0; i <= markdown.size(); ++i) {
      if (i == markdown.size() || markdown[i] == '\n') {
        rawLines.push_back(RStripCR(markdown.substr(start, i - start)));
        start = i + 1;
      }
    }
  }

  // Block model.
  enum class Kind { Paragraph, Heading, ListItem, Quote, Code, Rule, Image };
  struct Block {
    Kind kind;
    std::string text;              // raw inline text (Paragraph/Heading/ListItem/Quote)
    int headingLevel = 0;          // Heading
    std::vector<std::string> code; // Code lines
    std::string imgAlt, imgUrl;    // Image
  };
  std::vector<Block> blocks;

  bool inCode = false;
  Block codeBlock;
  std::string para;
  auto flushPara = [&]() {
    if (!para.empty()) {
      Block b;
      b.kind = Kind::Paragraph;
      b.text = para;
      blocks.push_back(b);
      para.clear();
    }
  };

  for (const std::string &lineRaw : rawLines) {
    std::string line = lineRaw;
    // Fenced code block toggle.
    std::string trimmedLeft = line;
    size_t lead = trimmedLeft.find_first_not_of(" ");
    std::string ltrim = (lead == std::string::npos) ? "" : trimmedLeft.substr(lead);
    if (ltrim.rfind("```", 0) == 0) {
      if (inCode) {
        blocks.push_back(codeBlock);
        codeBlock = Block();
        inCode = false;
      } else {
        flushPara();
        inCode = true;
        codeBlock = Block();
        codeBlock.kind = Kind::Code;
      }
      continue;
    }
    if (inCode) {
      codeBlock.code.push_back(line);
      continue;
    }

    if (ltrim.empty()) {
      flushPara();
      continue;
    }
    if (IsHRule(ltrim)) {
      flushPara();
      Block b;
      b.kind = Kind::Rule;
      blocks.push_back(b);
      continue;
    }
    // Image on its own line: ![alt](url)
    if (ltrim.rfind("![", 0) == 0) {
      size_t rb = ltrim.find(']', 2);
      if (rb != std::string::npos && rb + 1 < ltrim.size() && ltrim[rb + 1] == '(') {
        size_t rp = ltrim.find(')', rb + 2);
        if (rp != std::string::npos) {
          flushPara();
          Block b;
          b.kind = Kind::Image;
          b.imgAlt = ltrim.substr(2, rb - 2);
          b.imgUrl = ltrim.substr(rb + 2, rp - rb - 2);
          blocks.push_back(b);
          continue;
        }
      }
    }
    // Heading.
    if (ltrim[0] == '#') {
      int level = 0;
      while (level < (int)ltrim.size() && ltrim[level] == '#')
        ++level;
      if (level >= 1 && level <= 6 && level < (int)ltrim.size() &&
          ltrim[level] == ' ') {
        flushPara();
        Block b;
        b.kind = Kind::Heading;
        b.headingLevel = level;
        b.text = ltrim.substr(level + 1);
        blocks.push_back(b);
        continue;
      }
    }
    // Block quote.
    if (ltrim.rfind("> ", 0) == 0 || ltrim == ">") {
      flushPara();
      Block b;
      b.kind = Kind::Quote;
      b.text = ltrim.size() > 1 ? ltrim.substr(2) : "";
      blocks.push_back(b);
      continue;
    }
    // Unordered list item.
    if (ltrim.rfind("- ", 0) == 0 || ltrim.rfind("* ", 0) == 0 ||
        ltrim.rfind("+ ", 0) == 0) {
      flushPara();
      Block b;
      b.kind = Kind::ListItem;
      b.text = ltrim.substr(2);
      blocks.push_back(b);
      continue;
    }
    // Ordered list item: "N. ".
    {
      size_t d = 0;
      while (d < ltrim.size() && std::isdigit((unsigned char)ltrim[d]))
        ++d;
      if (d > 0 && d + 1 < ltrim.size() && ltrim[d] == '.' && ltrim[d + 1] == ' ') {
        flushPara();
        Block b;
        b.kind = Kind::ListItem;
        b.text = ltrim.substr(d + 2);
        blocks.push_back(b);
        continue;
      }
    }
    // Paragraph: accumulate (soft-wrap consecutive lines).
    if (!para.empty())
      para += " ";
    para += ltrim;
  }
  if (inCode)
    blocks.push_back(codeBlock);
  flushPara();

  // ── Layout: measure total height ────────────────────────────────────────
  float headingScale[7] = {1.0f, 1.9f, 1.55f, 1.3f, 1.15f, 1.05f, 1.0f};
  float blockGap = 6.0f;
  float listIndent = 18.0f;
  float quoteIndent = 14.0f;
  float lineH = baseFs * 1.45f;

  Vec2 origin = ctx->cursorPos;
  float totalH = 0.0f;
  for (const Block &b : blocks) {
    switch (b.kind) {
    case Kind::Rule:
      totalH += lineH * 0.6f + blockGap;
      break;
    case Kind::Code:
      totalH += static_cast<float>(b.code.size()) * (baseFs * 1.35f) + 12.0f + blockGap;
      break;
    case Kind::Image: {
      auto it = MdImages().find(b.imgUrl);
      float h = (it != MdImages().end() && it->second.size.y > 0) ? it->second.size.y
                                                                  : 80.0f;
      totalH += h + blockGap;
      break;
    }
    case Kind::Heading: {
      float fs = baseFs * headingScale[std::clamp(b.headingLevel, 1, 6)];
      totalH += MeasureInlineHeight(ctx, ParseInline(b.text), maxW, fs) + blockGap;
      break;
    }
    case Kind::ListItem:
      totalH += MeasureInlineHeight(ctx, ParseInline(b.text), maxW - listIndent, baseFs) + 2.0f;
      break;
    case Kind::Quote:
      totalH += MeasureInlineHeight(ctx, ParseInline(b.text), maxW - quoteIndent, baseFs) + 2.0f;
      break;
    case Kind::Paragraph:
    default:
      totalH += MeasureInlineHeight(ctx, ParseInline(b.text), maxW, baseFs) + blockGap;
      break;
    }
  }

  Vec2 totalSize(maxW, totalH);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  Vec2 pos = ctx->cursorPos;

  uint32_t wid = GenerateId("MDV:", id.c_str());

  // ── Draw ────────────────────────────────────────────────────────────────
  if (IsRectInViewport(ctx, pos, Vec2(finalSize.x, totalH))) {
    float y = pos.y;
    for (const Block &b : blocks) {
      switch (b.kind) {
      case Kind::Rule: {
        float ly = y + lineH * 0.3f;
        Color c = ctx->style.separator.color;
        ctx->renderer.DrawLine(Vec2(pos.x, ly), Vec2(pos.x + maxW, ly), c, 1.0f);
        y += lineH * 0.6f + blockGap;
        break;
      }
      case Kind::Code: {
        float codeFs = baseFs;
        float clH = baseFs * 1.35f;
        float h = static_cast<float>(b.code.size()) * clH + 12.0f;
        Color bg(0.5f, 0.5f, 0.5f, 0.14f);
        ctx->renderer.DrawRectFilled(Vec2(pos.x, y), Vec2(maxW, h), bg, 4.0f);
        float cy = y + 6.0f;
        Color codeCol(0.85f, 0.85f, 0.7f, 1.0f);
        for (const auto &cl : b.code) {
          ctx->renderer.DrawText(Vec2(pos.x + 8.0f, cy), cl, codeCol, codeFs);
          cy += clH;
        }
        y += h + blockGap;
        break;
      }
      case Kind::Image: {
        auto it = MdImages().find(b.imgUrl);
        if (it != MdImages().end() && it->second.handle) {
          Vec2 sz = it->second.size;
          if (sz.x > maxW && sz.x > 0) {
            float s = maxW / sz.x;
            sz = Vec2(maxW, sz.y * s);
          }
          ctx->renderer.DrawImage(Vec2(pos.x, y), sz, it->second.handle);
          y += sz.y + blockGap;
        } else {
          // Placeholder box with the alt text.
          float h = 80.0f;
          Color bg(0.5f, 0.5f, 0.5f, 0.12f);
          ctx->renderer.DrawRectFilled(Vec2(pos.x, y), Vec2(std::min(maxW, 200.0f), h),
                                       bg, 4.0f);
          ctx->renderer.DrawRect(Vec2(pos.x, y), Vec2(std::min(maxW, 200.0f), h),
                                 ctx->style.separator.color, 4.0f);
          std::string alt = b.imgAlt.empty() ? std::string("[image]") : ("[" + b.imgAlt + "]");
          Color c = baseColor;
          c.a *= 0.6f;
          ctx->renderer.DrawText(Vec2(pos.x + 8.0f, y + h * 0.5f - baseFs * 0.5f),
                                 alt, c, baseFs);
          y += h + blockGap;
        }
        break;
      }
      case Kind::Heading: {
        float fs = baseFs * headingScale[std::clamp(b.headingLevel, 1, 6)];
        float h = RenderInline(ctx, ParseInline(b.text), Vec2(pos.x, y), maxW, fs,
                               true, baseColor);
        y += h + blockGap;
        break;
      }
      case Kind::ListItem: {
        Color bullet = baseColor;
        // U+2022 (•) isn't in the MSDF atlas → it renders invisible via DrawText. Draw
        // the list bullet as a filled dot instead (same approach as PasswordBox),
        // vertically centered on the line.
        ctx->renderer.DrawCircle(Vec2(pos.x + 7.0f, y + lineH * 0.5f),
                                 baseFs * 0.14f, bullet, true);
        float h = RenderInline(ctx, ParseInline(b.text),
                               Vec2(pos.x + listIndent, y), maxW - listIndent, baseFs,
                               false, baseColor);
        y += h + 2.0f;
        break;
      }
      case Kind::Quote: {
        float h = MeasureInlineHeight(ctx, ParseInline(b.text), maxW - quoteIndent, baseFs);
        Color bar = FluentColors::Accent;
        bar.a = 0.6f;
        ctx->renderer.DrawRectFilled(Vec2(pos.x, y), Vec2(3.0f, h), bar, 1.0f);
        Color qc = baseColor;
        qc.a *= 0.8f;
        RenderInline(ctx, ParseInline(b.text), Vec2(pos.x + quoteIndent, y),
                     maxW - quoteIndent, baseFs, false, qc);
        y += h + 2.0f;
        break;
      }
      case Kind::Paragraph:
      default: {
        float h = RenderInline(ctx, ParseInline(b.text), Vec2(pos.x, y), maxW, baseFs,
                               false, baseColor);
        y += h + blockGap;
        break;
      }
      }
    }
  }

  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, Vec2(finalSize.x, totalH));
  SetLastItem(wid, pos, pos + Vec2(finalSize.x, totalH), false, false, false, false);
}

} // namespace FluentUI
