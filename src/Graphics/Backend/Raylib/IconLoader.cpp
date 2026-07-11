#include "RaylibBackend.h"

#include "raylib.h"

#include <cstdio>

#ifdef UI_ENABLE_SVG
#include <cstdlib>
#include "nanosvg.h"
#include "nanosvgrast.h"
#endif

// Icon loading for the Raylib backend, preferring SVG (rasterized crisply at the
// target pixel size, via nanosvg) over PNG. Monochrome icons then recolor through
// the existing icon tint. When UI_ENABLE_SVG is off, this is PNG-only.
namespace Ui::Raylib {

#ifdef UI_ENABLE_SVG
static Texture2D RasterizeSvg(const char* path, int pixelSize)
{
    NSVGimage* image = nsvgParseFromFile(path, "px", 96.0f);
    if (!image) {
        return Texture2D {};
    }
    float dim = image->width > image->height ? image->width : image->height;
    float scale = dim > 0.0f ? static_cast<float>(pixelSize) / dim : 1.0f;

    Texture2D tex {};
    unsigned char* pixels = static_cast<unsigned char*>(std::malloc(static_cast<size_t>(pixelSize) * pixelSize * 4));
    if (pixels) {
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        nsvgRasterize(rast, image, 0.0f, 0.0f, scale, pixels, pixelSize, pixelSize, pixelSize * 4);
        Image img { pixels, pixelSize, pixelSize, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        tex = LoadTextureFromImage(img); // uploads to GPU; pixels can be freed after.
        nsvgDeleteRasterizer(rast);
        std::free(pixels);
    }
    nsvgDelete(image);
    return tex;
}
#endif

Texture2D LoadIcon(const char* basePathNoExt, int pixelSize)
{
    char path[512];

#ifdef UI_ENABLE_SVG
    std::snprintf(path, sizeof(path), "%s.svg", basePathNoExt);
    if (FileExists(path)) {
        Texture2D tex = RasterizeSvg(path, pixelSize);
        if (tex.id != 0) {
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            return tex;
        }
    }
#endif

    std::snprintf(path, sizeof(path), "%s.png", basePathNoExt);
    if (FileExists(path)) {
        Texture2D tex = LoadTexture(path);
        if (tex.id != 0) {
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
        }
        return tex;
    }
    return Texture2D {};
}

} // namespace Ui::Raylib
