// Waypoint Engine — the stb_image implementation.
// SPDX-License-Identifier: Apache-2.0
//
// stb is header-only, which means exactly one translation unit per binary has to
// define the implementation macro. tinygltf's inline helpers call stb_image
// directly, so this file exists to satisfy them: without it the engine library
// has undefined references to stbi_load and friends that only show up at link
// time, and only in binaries that pull in the loader.
//
// The write side (stb_image_write) lives in core/ImageWrite.cpp, which is the
// only place that writes images.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
