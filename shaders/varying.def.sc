// Declares the vertex attributes and varyings shared by the engine's shaders.
// bgfx includes this file for every shader so the names always line up.
vec3 a_position : POSITION;
vec4 a_color0   : COLOR0;

// Inputs are vertex attributes, outputs are interpolated varyings; they must not
// share an HLSL semantic, or DXC folds the store into the input register.
vec4 v_color0   : TEXCOORD0;
