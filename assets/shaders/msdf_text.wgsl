// Multi-channel signed distance field text.
//
// Glyphs are billboarded quads anchored at a world position but sized in
// pixels, so viewport labels stay legible at any zoom while still being
// occluded by geometry in front of them. The MSDF encoding keeps corners sharp
// at magnifications where a plain alpha atlas would go soft.

struct Uniforms {
    viewProj     : mat4x4<f32>,
    viewportSize : vec4<f32>,  // xy = pixels, z = distance field range in px, w = unused
};

@group(0) @binding(0) var<uniform> u : Uniforms;
@group(0) @binding(1) var atlasTexture : texture_2d<f32>;
@group(0) @binding(2) var atlasSampler : sampler;

struct VSIn {
    @location(0) anchor : vec3<f32>,  // world-space origin of this glyph's text run
    @location(1) offset : vec2<f32>,  // pixel offset of this corner from the anchor
    @location(2) uv     : vec2<f32>,
    @location(3) color  : vec4<f32>,
};

struct VSOut {
    @builtin(position) position : vec4<f32>,
    @location(0)       uv       : vec2<f32>,
    @location(1)       color    : vec4<f32>,
};

@vertex
fn vs_main(in : VSIn) -> VSOut {
    var clip = u.viewProj * vec4<f32>(in.anchor, 1.0);

    // Scaling the pixel offset by clip.w cancels the perspective divide, which
    // is what holds the glyph at a fixed on-screen size regardless of depth.
    let ndcPerPixel = vec2<f32>(2.0, -2.0) / u.viewportSize.xy;
    clip = vec4<f32>(clip.xy + in.offset * ndcPerPixel * clip.w, clip.z, clip.w);

    var out : VSOut;
    out.position = clip;
    out.uv = in.uv;
    out.color = in.color;
    return out;
}

fn median(v : vec3<f32>) -> f32 {
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    let sample = textureSample(atlasTexture, atlasSampler, in.uv).rgb;
    let signedDistance = median(sample) - 0.5;

    // Convert the distance from atlas units into screen pixels. Deriving the
    // scale from fwidth keeps the edge exactly one pixel wide under any
    // projection, rotation or zoom.
    let atlasSize = vec2<f32>(textureDimensions(atlasTexture, 0));
    let unitRange = vec2<f32>(u.viewportSize.z) / atlasSize;
    let screenTexSize = vec2<f32>(1.0) / max(fwidth(in.uv), vec2<f32>(1e-8));
    let screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    let opacity = clamp(signedDistance * screenPxRange + 0.5, 0.0, 1.0);
    if (opacity <= 0.001) {
        discard;
    }

    let alpha = in.color.a * opacity;
    return vec4<f32>(in.color.rgb * alpha, alpha);  // premultiplied
}
