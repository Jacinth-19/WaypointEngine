// Waypoint Engine — basic fragment shader.
// SPDX-License-Identifier: Apache-2.0
//
// Vertex colour multiplied by a per-draw tint, so one cube can be grey, orange
// or red without a separate mesh. Lighting arrives in a later milestone.
$input v_color0

#include <bgfx_shader.sh>

uniform vec4 u_tint;

void main()
{
    gl_FragColor = v_color0 * u_tint;
}
