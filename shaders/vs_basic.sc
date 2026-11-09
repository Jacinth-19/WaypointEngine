// Waypoint Engine — basic vertex shader (position, normal, vertex colour).
// SPDX-License-Identifier: Apache-2.0
//
// u_modelViewProj is a bgfx built-in uniform: it is declared for us inside
// bgfx_shader.sh, so no uniform handle is needed in C++.
$input a_position, a_normal, a_color0
$output v_color0, v_normal

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_color0 = a_color0;
    // u_model is declared as an array (for skinning); without bones the model
    // matrix is element 0. The matrix carries no non-uniform scale today, so
    // transforming by it directly is correct - if scaling ever becomes
    // non-uniform this needs the inverse transpose instead.
    v_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
}
