// Waypoint Engine — basic fragment shader.
// SPDX-License-Identifier: Apache-2.0
//
// Vertex colour times a per-draw tint, times one directional light: Lambert
// diffuse with a small ambient term so a face turned away from the light is
// dark rather than black. Materials, more lights and shadows come later; the
// point of this step is that DirectionalLightComponent stops being ignored.
$input v_color0, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_tint;
// xyz = direction the light travels (towards the scene), w = intensity.
uniform vec4 u_lightDirection;
// rgb = light colour, a = ambient amount.
uniform vec4 u_lightColor;

void main()
{
    // Check the length *before* normalizing: normalize(vec3(0.0)) is NaN, and a
    // NaN normal would black out or blank out the surface depending on the GPU.
    vec3 normal = v_normal;
    normal = dot(normal, normal) > 0.0 ? normalize(normal) : vec3(0.0, 0.0, 1.0);

    // u_lightDirection points the way the light travels, so the surface facing
    // the light has its normal against it.
    const float lambert = max(dot(normal, -u_lightDirection.xyz), 0.0);
    const vec3 diffuse = u_lightColor.rgb * u_lightDirection.w * lambert;
    const vec3 ambient = u_lightColor.rgb * u_lightColor.a;

    gl_FragColor = vec4(v_color0.rgb * u_tint.rgb * (ambient + diffuse), v_color0.a * u_tint.a);
}
