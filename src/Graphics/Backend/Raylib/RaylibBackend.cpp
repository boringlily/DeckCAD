#include "RaylibBackend.h"

#include "raylib.h"

#include <cmath>
#include <cstring>

// Raylib default backend. The text-measurement math is ported from the existing
// Graphics::Raylib_MeasureText (glyph advanceX/recs, fontSize/baseSize scaling,
// newline handling), with the Clay string slice swapped for (chars, len).
namespace Ui::Raylib {

namespace {

    Color ToRaylib(UiColor c)
    {
        return Color { c.r, c.g, c.b, c.a };
    }

    Font& FontFor(State* state, u16 fontId)
    {
        static Font fallback = GetFontDefault();
        if (!state || !state->fonts || fontId >= state->fontCount || !state->fonts[fontId].glyphs) {
            // Lazily grab the default font (raylib must be initialised by now).
            if (!fallback.glyphs) {
                fallback = GetFontDefault();
            }
            return fallback;
        }
        return state->fonts[fontId];
    }

    // Horizontal advance of one glyph in the font's base units (unscaled).
    f32 GlyphAdvance(const Font& font, char ch)
    {
        int index = ch - 32;
        if (index < 0 || index >= font.glyphCount) {
            return 0.0f;
        }
        if (font.glyphs[index].advanceX != 0) {
            return static_cast<float>(font.glyphs[index].advanceX);
        }
        return font.recs[index].width + static_cast<float>(font.glyphs[index].offsetX);
    }

    // Break `text` into lines that fit `maxWidth` (scaled px), wrapping at spaces
    // and honoring explicit newlines. Calls emit(start, count) per line; returns
    // the line count.
    template <typename EmitLine>
    u32 ForEachWrappedLine(const Font& font, const char* text, u32 len, float scale, float maxWidth, EmitLine emit)
    {
        if (maxWidth < 1.0f) {
            maxWidth = 1.0f;
        }
        u32 lineStart = 0;
        float lineW = 0;
        u32 lastSpace = u32_max;
        u32 lineCount = 0;
        auto runWidth = [&](u32 a, u32 b) {
            float w = 0;
            for (u32 k = a; k < b; ++k) {
                w += GlyphAdvance(font, text[k]) * scale;
            }
            return w;
        };
        for (u32 i = 0; i < len; ++i) {
            char c = text[i];
            if (c == '\n') {
                emit(lineStart, i - lineStart);
                ++lineCount;
                lineStart = i + 1;
                lineW = 0;
                lastSpace = u32_max;
                continue;
            }
            float cw = GlyphAdvance(font, c) * scale;
            if (lineW + cw > maxWidth && i > lineStart) {
                if (lastSpace != u32_max && lastSpace >= lineStart) {
                    emit(lineStart, lastSpace - lineStart);
                    lineStart = lastSpace + 1;
                    lineW = runWidth(lineStart, i);
                    lastSpace = u32_max;
                } else {
                    emit(lineStart, i - lineStart);
                    lineStart = i;
                    lineW = 0;
                    lastSpace = u32_max;
                }
                ++lineCount;
            }
            if (c == ' ') {
                lastSpace = i;
            }
            lineW += cw;
        }
        emit(lineStart, len - lineStart);
        ++lineCount;
        return lineCount;
    }

    TextMetrics MeasureText(void* user, const char* text, u32 len, u16 fontId, u16 fontSize)
    {
        State* state = static_cast<State*>(user);
        Font font = FontFor(state, fontId);

        float maxLineWidth = 0.0f;
        float lineWidth = 0.0f;
        u32 lines = 1;
        const float scale = static_cast<float>(fontSize) / static_cast<float>(font.baseSize);

        for (u32 i = 0; i < len; ++i) {
            if (text[i] == '\n') {
                maxLineWidth = fmaxf(maxLineWidth, lineWidth);
                lineWidth = 0.0f;
                ++lines;
                continue;
            }
            lineWidth += GlyphAdvance(font, text[i]);
        }
        maxLineWidth = fmaxf(maxLineWidth, lineWidth);

        return TextMetrics {
            .width = maxLineWidth * scale,
            .height = static_cast<float>(fontSize) * static_cast<float>(lines),
        };
    }

    void DrawText(void* user, const char* text, u32 len, Vec2 pos, u16 fontId, u16 fontSize, UiColor color)
    {
        State* state = static_cast<State*>(user);
        Font font = FontFor(state, fontId);

        // raylib needs a null-terminated C string; clone into a bounded buffer.
        char stackBuf[512];
        u32 n = len < sizeof(stackBuf) - 1 ? len : static_cast<u32>(sizeof(stackBuf) - 1);
        memcpy(stackBuf, text, n);
        stackBuf[n] = '\0';

        // Snap to the nearest integer pixel: the font atlas is bilinear-filtered
        // (LoadAppFonts), so a glyph quad drawn at a fractional position - which
        // the layout engine produces routinely (sub-pixel glyph advances, Grow
        // division, *0.5f centering) - gets GPU-resampled against neighboring
        // texels and reads as blurry. Every other primitive here (DrawRect,
        // DrawBorder, icons) already snaps its position the same way.
        DrawTextEx(font, stackBuf, Vector2 { roundf(pos.x), roundf(pos.y) }, static_cast<float>(fontSize), 0.0f, ToRaylib(color));
    }

    TextMetrics MeasureWrapped(void* user, const char* text, u32 len, u16 fontId, u16 fontSize, f32 maxWidth)
    {
        State* state = static_cast<State*>(user);
        Font font = FontFor(state, fontId);
        const float scale = static_cast<float>(fontSize) / static_cast<float>(font.baseSize);
        u32 lines = ForEachWrappedLine(font, text, len, scale, maxWidth, [](u32, u32) {});
        return TextMetrics {
            .width = maxWidth,
            .height = static_cast<float>(fontSize) * static_cast<float>(lines),
        };
    }

    // Scaled width of the glyph run [a, b).
    float RunWidth(const Font& font, const char* text, u32 a, u32 b, float scale)
    {
        float w = 0;
        for (u32 k = a; k < b; ++k) {
            w += GlyphAdvance(font, text[k]) * scale;
        }
        return w;
    }

    // Byte boundary in [a, b] nearest `targetX` (relative to the run start).
    u32 ByteInRun(const Font& font, const char* text, u32 a, u32 b, float scale, float targetX)
    {
        if (targetX <= 0) {
            return a;
        }
        float x = 0;
        for (u32 i = a; i < b; ++i) {
            float w = GlyphAdvance(font, text[i]) * scale;
            if (targetX < x + w * 0.5f) {
                return i;
            }
            x += w;
        }
        return b;
    }

    void DrawWrapped(void* user, const char* text, u32 len, Rect box, u16 fontId, u16 fontSize, UiColor color, s32 caretByte,
        u32 selStart, u32 selEnd, UiColor selColor)
    {
        State* state = static_cast<State*>(user);
        Font font = FontFor(state, fontId);
        const float scale = static_cast<float>(fontSize) / static_cast<float>(font.baseSize);
        Color c = ToRaylib(color);
        const float lineH = static_cast<float>(fontSize);
        if (selStart > len) {
            selStart = len; // clamp the selection range to the drawn text.
        }
        if (selEnd > len) {
            selEnd = len;
        }
        u32 cb = caretByte >= 0 ? (static_cast<u32>(caretByte) > len ? len : static_cast<u32>(caretByte)) : u32_max;
        int caretLine = -1;
        float caretX = 0.0f;
        u32 lineIdx = 0;
        ForEachWrappedLine(font, text, len, scale, box.w, [&](u32 start, u32 count) {
            float ly = box.y + static_cast<float>(lineIdx) * lineH;
            // Selection highlight for this line's covered range.
            if (selEnd > selStart) {
                u32 a = selStart > start ? selStart : start;
                u32 b = selEnd < start + count ? selEnd : start + count;
                if (b > a) {
                    float xa = RunWidth(font, text, start, a, scale);
                    float xb = RunWidth(font, text, start, b, scale);
                    DrawRectangle(static_cast<int>(box.x + xa), static_cast<int>(ly),
                        static_cast<int>(xb - xa), static_cast<int>(lineH), ToRaylib(selColor));
                }
            }
            char buf[512];
            u32 nn = count < sizeof(buf) - 1 ? count : static_cast<u32>(sizeof(buf) - 1);
            memcpy(buf, text + start, nn);
            buf[nn] = '\0';
            DrawTextEx(font, buf, Vector2 { roundf(box.x), roundf(ly) }, lineH, 0.0f, c); // pixel-snapped; see DrawText.
            // Locate the caret's wrapped line + x (first line whose range covers it).
            if (caretByte >= 0 && caretLine < 0 && cb >= start && cb <= start + count) {
                caretLine = static_cast<int>(lineIdx);
                caretX = RunWidth(font, text, start, cb, scale);
            }
            ++lineIdx;
        });
        if (caretByte >= 0) {
            if (caretLine < 0) {
                caretLine = lineIdx > 0 ? static_cast<int>(lineIdx) - 1 : 0;
            }
            DrawRectangle(static_cast<int>(box.x + caretX), static_cast<int>(box.y + static_cast<float>(caretLine) * lineH),
                1, static_cast<int>(lineH), c);
        }
    }

    void DrawStyled(void* user, const char* text, u32 len, Rect box, u16 fontId, u16 fontSize,
        UiColor defaultColor, const TextStyleRun* runs, u32 runCount, s32 caretByte,
        u32 selStart, u32 selEnd, UiColor selColor)
    {
        State* state = static_cast<State*>(user);
        Font font = FontFor(state, fontId);
        const float scale = static_cast<float>(fontSize) / static_cast<float>(font.baseSize);

        // Horizontal scroll-to-caret: shift the line left so the caret stays inside
        // the box (the input clips the overflow). `box.w` is the view width.
        float scrollX = 0.0f;
        if (caretByte >= 0) {
            u32 cb = static_cast<u32>(caretByte) > len ? len : static_cast<u32>(caretByte);
            float caretX = RunWidth(font, text, 0, cb, scale);
            float margin = static_cast<float>(fontSize) * 0.5f;
            if (caretX > box.w - margin) {
                scrollX = caretX - (box.w - margin);
            }
        }
        float x = box.x - scrollX;
        const float y = box.y;

        // Selection highlight (behind the text); clamp the range to the drawn text.
        {
            u32 a = selStart < len ? selStart : len;
            u32 b = selEnd < len ? selEnd : len;
            if (b > a) {
                float xa = RunWidth(font, text, 0, a, scale);
                float xb = RunWidth(font, text, 0, b, scale);
                DrawRectangle(static_cast<int>(box.x - scrollX + xa), static_cast<int>(y),
                    static_cast<int>(xb - xa), static_cast<int>(fontSize), ToRaylib(selColor));
            }
        }

        // Draw [a,b) of text in `color` at the running x; optionally underline it.
        auto drawSeg = [&](u32 a, u32 b, UiColor color, TextDecoration decoration) {
            if (b <= a) {
                return;
            }
            char buf[512];
            u32 nn = (b - a) < sizeof(buf) - 1 ? (b - a) : static_cast<u32>(sizeof(buf) - 1);
            memcpy(buf, text + a, nn);
            buf[nn] = '\0';
            float w = 0;
            for (u32 k = a; k < b; ++k) {
                w += GlyphAdvance(font, text[k]) * scale;
            }
            DrawTextEx(font, buf, Vector2 { roundf(x), roundf(y) }, static_cast<float>(fontSize), 0.0f, ToRaylib(color)); // pixel-snapped; see DrawText.
            if (decoration != TextDecoration::None) {
                int thickness = decoration == TextDecoration::Error ? 2 : 1;
                DrawRectangle(static_cast<int>(x), static_cast<int>(y + fontSize - 2),
                    static_cast<int>(w), thickness, ToRaylib(color));
            }
            x += w;
        };

        u32 cursor = 0;
        for (u32 i = 0; i < runCount; ++i) {
            u32 rs = runs[i].start > len ? len : runs[i].start;
            u32 re = runs[i].start + runs[i].length;
            if (re > len) {
                re = len;
            }
            if (rs < cursor) { // skip overlapping/out-of-order runs.
                continue;
            }
            if (rs > cursor) {
                drawSeg(cursor, rs, defaultColor, TextDecoration::None);
            }
            drawSeg(rs, re, runs[i].color, runs[i].decoration);
            cursor = re;
        }
        if (cursor < len) {
            drawSeg(cursor, len, defaultColor, TextDecoration::None);
        }

        // Editing caret: a thin bar at the glyph boundary `caretByte` (scrolled).
        if (caretByte >= 0) {
            u32 cb = static_cast<u32>(caretByte) > len ? len : static_cast<u32>(caretByte);
            float cx = box.x - scrollX;
            for (u32 k = 0; k < cb; ++k) {
                cx += GlyphAdvance(font, text[k]) * scale;
            }
            DrawRectangle(static_cast<int>(cx), static_cast<int>(y), 1,
                static_cast<int>(fontSize), ToRaylib(defaultColor));
        }
    }

    u32 CaretIndexAt(void* user, const char* text, u32 len, Rect box, u16 fontId, u16 fontSize, s32 caretByte, Vec2 point, bool wrap)
    {
        State* state = static_cast<State*>(user);
        Font font = FontFor(state, fontId);
        const float scale = static_cast<float>(fontSize) / static_cast<float>(font.baseSize);

        if (wrap) {
            float lineH = static_cast<float>(fontSize);
            int targetLine = static_cast<int>((point.y - box.y) / lineH);
            if (targetLine < 0) {
                targetLine = 0;
            }
            u32 result = len; // clicked below the last line -> end of text.
            bool done = false;
            u32 lineIdx = 0;
            ForEachWrappedLine(font, text, len, scale, box.w, [&](u32 start, u32 count) {
                if (done) {
                    return;
                }
                if (static_cast<int>(lineIdx) == targetLine) {
                    result = ByteInRun(font, text, start, start + count, scale, point.x - box.x);
                    done = true;
                }
                ++lineIdx;
            });
            return result;
        }

        // Single-line: account for the same scroll-to-caret offset used when drawing.
        float scrollX = 0.0f;
        if (caretByte >= 0) {
            u32 cb = static_cast<u32>(caretByte) > len ? len : static_cast<u32>(caretByte);
            float cx = RunWidth(font, text, 0, cb, scale);
            float margin = static_cast<float>(fontSize) * 0.5f;
            if (cx > box.w - margin) {
                scrollX = cx - (box.w - margin);
            }
        }
        return ByteInRun(font, text, 0, len, scale, point.x - box.x + scrollX);
    }

    void ClipboardSet(void* /*user*/, const char* text) { SetClipboardText(text); }
    const char* ClipboardGet(void* /*user*/) { return GetClipboardText(); }

    void DrawRect(void* /*user*/, Rect box, UiColor color, f32 cornerRadius)
    {
        if (cornerRadius > 0.0f) {
            float shortSide = box.w < box.h ? box.w : box.h;
            float roundness = shortSide > 0.0f ? (cornerRadius * 2.0f) / shortSide : 0.0f;
            DrawRectangleRounded(Rectangle { box.x, box.y, box.w, box.h }, roundness, 8, ToRaylib(color));
        } else {
            DrawRectangle(static_cast<int>(box.x), static_cast<int>(box.y),
                static_cast<int>(box.w), static_cast<int>(box.h), ToRaylib(color));
        }
    }

    void DrawImage(void* /*user*/, void* imageHandle, Rect dst, UiColor tint)
    {
        if (!imageHandle) {
            return;
        }
        Texture2D tex = *static_cast<Texture2D*>(imageHandle);
        DrawTexturePro(tex,
            Rectangle { 0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height) },
            Rectangle { dst.x, dst.y, dst.w, dst.h },
            Vector2 { 0, 0 }, 0.0f, ToRaylib(tint));
    }

    void DrawIcon(void* user, u32 iconId, Rect dst, UiColor tint)
    {
        State* state = static_cast<State*>(user);
        if (!state || !state->icons || iconId >= state->iconCount) {
            return;
        }
        Texture2D tex = state->icons[iconId];
        DrawTexturePro(tex,
            Rectangle { 0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height) },
            Rectangle { dst.x, dst.y, dst.w, dst.h },
            Vector2 { 0, 0 }, 0.0f, ToRaylib(tint));
    }

    // Per-side border, with rounded corner arcs when cornerRadius > 0. Ported from
    // the border drawing in the existing Graphics::Render.
    void DrawBorder(void* /*user*/, Rect box, Edges w, UiColor color, f32 radius)
    {
        Color c = ToRaylib(color);
        if (w.left > 0) {
            DrawRectangle((int)box.x, (int)(box.y + radius), (int)w.left, (int)(box.h - radius * 2), c);
        }
        if (w.right > 0) {
            DrawRectangle((int)(box.x + box.w - w.right), (int)(box.y + radius), (int)w.right, (int)(box.h - radius * 2), c);
        }
        if (w.top > 0) {
            DrawRectangle((int)(box.x + radius), (int)box.y, (int)(box.w - radius * 2), (int)w.top, c);
        }
        if (w.bottom > 0) {
            DrawRectangle((int)(box.x + radius), (int)(box.y + box.h - w.bottom), (int)(box.w - radius * 2), (int)w.bottom, c);
        }
        if (radius > 0) {
            DrawRing(Vector2 { box.x + radius, box.y + radius }, radius - w.top, radius, 180, 270, 12, c);
            DrawRing(Vector2 { box.x + box.w - radius, box.y + radius }, radius - w.top, radius, 270, 360, 12, c);
            DrawRing(Vector2 { box.x + radius, box.y + box.h - radius }, radius - w.bottom, radius, 90, 180, 12, c);
            DrawRing(Vector2 { box.x + box.w - radius, box.y + box.h - radius }, radius - w.bottom, radius, 0, 90, 12, c);
        }
    }

    void ScissorStart(void* /*user*/, Rect box)
    {
        BeginScissorMode(static_cast<int>(box.x), static_cast<int>(box.y),
            static_cast<int>(box.w), static_cast<int>(box.h));
    }

    void ScissorEnd(void* /*user*/)
    {
        EndScissorMode();
    }

} // namespace

UiBackend MakeBackend(State* state, ColorScheme colors)
{
    UiBackend backend {};
    backend.draw.user = state;
    backend.draw.FillRect = DrawRect;
    backend.draw.Border = DrawBorder;
    backend.draw.ScissorStart = ScissorStart;
    backend.draw.ScissorEnd = ScissorEnd;

    backend.text.user = state;
    backend.text.Measure = MeasureText;
    backend.text.Draw = DrawText;
    backend.text.MeasureWrapped = MeasureWrapped;
    backend.text.DrawWrapped = DrawWrapped;
    backend.text.DrawStyled = DrawStyled;
    backend.text.CaretIndexAt = CaretIndexAt;

    backend.clipboard.Set = ClipboardSet;
    backend.clipboard.Get = ClipboardGet;

    backend.image.user = state;
    backend.image.Draw = DrawImage;

    backend.icon.user = state;
    backend.icon.Draw = DrawIcon;

    backend.colors = colors;
    return backend;
}

} // namespace Ui::Raylib
