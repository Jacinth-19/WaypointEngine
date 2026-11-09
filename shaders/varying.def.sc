// Declares the vertex attributes and varyings shared by the engine's shaders.
// bgfx includes this file for every shader so the names always line up.
vec3 a_position : POSITION;
vec3 a_normal   : NORMAL;
vec4 a_color0   : COLOR0;

// Inputs are vertex attributes, outputs are interpolated varyings; they must not
// share an HLSL semantic, or DXC folds the store into the input register.
// v_normal therefore starts at TEXCOORD1: TEXCOORD0 is taken by v_color0.
vec4 v_color0   : TEXCOORD0;
vec3 v_normal   : TEXCOORD1;
