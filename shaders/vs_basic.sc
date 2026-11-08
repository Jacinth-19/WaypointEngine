// Waypoint Engine — basic vertex shader (position + vertex colour).
// SPDX-License-Identifier: Apache-2.0
//
// u_modelViewProj is a bgfx built-in uniform: it is declared for us inside
// bgfx_shader.sh, so no uniform handle is needed in C++.
$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_color0 = a_color0;
}
