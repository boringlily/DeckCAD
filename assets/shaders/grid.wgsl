// Analytic infinite ground grid.
//
// Rather than pushing line geometry, this rasterizes one full-screen triangle
// and, per pixel, intersects the view ray with the Y=0 plane. Grid lines are
// derived from the world-space hit position and anti-aliased with screen-space
// derivatives, so the grid stays crisp at every zoom level and costs the same
// no matter how far it extends.

struct Uniforms {
    viewProj    : mat4x4<f32>,
    invViewProj : mat4x4<f32>,
    cameraPos   : vec4<f32>,  // xyz = world position
    params      : vec4<f32>,  // x = minor spacing, y = major every N minors, z = fade start, w = fade end
    colorMinor  : vec4<f32>,
    colorMajor  : vec4<f32>,
    colorAxisX  : vec4<f32>,
    colorAxisZ  : vec4<f32>,
};

@group(0) @binding(0) var<uniform> u : Uniforms;

struct VSOut {
    @builtin(position) position : vec4<f32>,
    @location(0)       ndc      : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex : u32) -> VSOut {
    // One oversized triangle covering the viewport; cheaper than a quad and
    // avoids the diagonal seam two triangles would introduce.
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>(-1.0,  1.0),
        vec2<f32>( 3.0,  1.0),
    );

    let p = positions[vertexIndex];
    var out : VSOut;
    out.position = vec4<f32>(p, 0.0, 1.0);
    out.ndc = p;
    return out;
}

/// Coverage of a line grid of the given spacing, anti-aliased to ~1px.
fn gridCoverage(coord : vec2<f32>, spacing : f32) -> f32 {
    let c = coord / spacing;
    // fwidth gives how much `c` changes across one pixel, which is what turns a
    // hard line test into a smooth one at any distance.
    let deriv = fwidth(c);
    let dist = abs(fract(c - 0.5) - 0.5) / max(deriv, vec2<f32>(1e-8));
    let line = min(dist.x, dist.y);
    return 1.0 - min(line, 1.0);
}

/// Coverage of a single axis line at value == 0.
fn axisCoverage(value : f32) -> f32 {
    let deriv = max(fwidth(value), 1e-8);
    return 1.0 - min(abs(value) / deriv, 1.0);
}

struct FSOut {
    @location(0)          color : vec4<f32>,
    @builtin(frag_depth)  depth : f32,
};

@fragment
fn fs_main(in : VSOut) -> FSOut {
    // Rebuild the world-space ray for this pixel from the inverse view-projection.
    let nearH = u.invViewProj * vec4<f32>(in.ndc, 0.0, 1.0);
    let farH  = u.invViewProj * vec4<f32>(in.ndc, 1.0, 1.0);
    let nearP = nearH.xyz / nearH.w;
    let farP  = farH.xyz  / farH.w;

    let dir = farP - nearP;

    // Ray parallel to the ground plane never hits it.
    if (abs(dir.y) < 1e-7) {
        discard;
    }

    let t = -nearP.y / dir.y;
    if (t < 0.0 || t > 1.0) {
        discard;  // plane is behind the eye or beyond the far plane
    }

    let hit = nearP + dir * t;

    let minorSpacing = u.params.x;
    let majorSpacing = minorSpacing * u.params.y;

    let minor = gridCoverage(hit.xz, minorSpacing);
    let major = gridCoverage(hit.xz, majorSpacing);

    // Major lines sit on top of minor ones.
    var color = u.colorMinor;
    color.a = color.a * minor;
    color = mix(color, u.colorMajor, major);

    // The two world axes take precedence over both.
    let axisX = axisCoverage(hit.z);  // the X axis runs along z == 0
    let axisZ = axisCoverage(hit.x);  // the Z axis runs along x == 0
    color = mix(color, u.colorAxisX, axisX);
    color = mix(color, u.colorAxisZ, axisZ);

    if (color.a <= 0.001) {
        discard;
    }

    // Fade the grid out with distance so the horizon does not alias into moire.
    let distance = length(hit - u.cameraPos.xyz);
    let fade = 1.0 - smoothstep(u.params.z, u.params.w, distance);
    if (fade <= 0.001) {
        discard;
    }
    color.a = color.a * fade;

    // Write true depth so scene geometry occludes the grid correctly.
    let clip = u.viewProj * vec4<f32>(hit, 1.0);

    var out : FSOut;
    out.color = vec4<f32>(color.rgb * color.a, color.a);  // premultiplied
    out.depth = clip.z / clip.w;
    return out;
}
