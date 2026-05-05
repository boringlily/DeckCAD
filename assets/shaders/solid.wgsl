// Flat vertex-coloured geometry: origin planes, and a starting point for any
// future solid/wireframe pass in the viewport.

struct Uniforms {
    viewProj : mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> u : Uniforms;

struct VSIn {
    @location(0) position : vec3<f32>,
    @location(1) color    : vec4<f32>,
};

struct VSOut {
    @builtin(position) position : vec4<f32>,
    @location(0)       color    : vec4<f32>,
};

@vertex
fn vs_main(in : VSIn) -> VSOut {
    var out : VSOut;
    out.position = u.viewProj * vec4<f32>(in.position, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color.rgb * in.color.a, in.color.a);  // premultiplied
}
