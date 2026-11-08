// Waypoint Engine — bgfx renderer implementation.
// SPDX-License-Identifier: Apache-2.0
//
// This file is where the pinned bgfx API (1.167) differs from older tutorials:
//   * the platform handle lives in `Init::swapChain`, not `PlatformData`;
//   * handle type lives in `PlatformData::type` (Wayland must opt in);
//   * resizing is `bgfx::reset(flags, &swapChain)`;
//   * `u_modelViewProj` is a built-in uniform: no handle is needed;
//   * depth convention depends on `bgfx::getCaps()->homogeneousDepth`.
#include "render/BgfxRenderer.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/FileSystem.hpp"
#include "core/Log.hpp"

#if defined(WAYPOINT_SHADER_DIR)
#define WAYPOINT_COMPILED_SHADER_DIR WAYPOINT_SHADER_DIR
#else
#define WAYPOINT_COMPILED_SHADER_DIR ""
#endif

namespace waypoint {
namespace {

// Position + packed colour, 16 bytes. bgfx expects ABGR packed into a uint32
// on little-endian machines, hence the unusual shift order.
struct PosColorVertex {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    u32 abgr = 0xFFFFFFFFu;
};
static_assert(sizeof(PosColorVertex) == 16, "vertex must stay tightly packed");

u32 abgr(f32 r, f32 g, f32 b, f32 a = 1.0f) {
    const auto toByte = [](f32 value) {
        const f32 clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        return static_cast<u32>(clamped * 255.0f + 0.5f);
    };
    return (toByte(a) << 24) | (toByte(b) << 16) | (toByte(g) << 8) | toByte(r);
}

// A unit cube made of six coloured faces, so the shape reads without lighting.
const std::array<PosColorVertex, 24>& cubeVertices() {
    static const std::array<PosColorVertex, 24> vertices = [] {
        std::array<PosColorVertex, 24> result{};
        const f32 h = 0.5f;
        // Six faces; each face gets a slightly different brightness.
        const f32 shades[6] = {1.0f, 0.82f, 0.68f, 0.9f, 0.76f, 0.6f};
        const f32 positions[6][4][3] = {
            // +X
            {{h, -h, -h}, {h, h, -h}, {h, h, h}, {h, -h, h}},
            // -X
            {{-h, -h, h}, {-h, h, h}, {-h, h, -h}, {-h, -h, -h}},
            // +Y
            {{-h, h, -h}, {-h, h, h}, {h, h, h}, {h, h, -h}},
            // -Y
            {{-h, -h, h}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}},
            // +Z
            {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},
            // -Z
            {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}},
        };

        for (int face = 0; face < 6; ++face) {
            for (int vertex = 0; vertex < 4; ++vertex) {
                PosColorVertex& out = result[static_cast<std::size_t>(face * 4 + vertex)];
                out.x = positions[face][vertex][0];
                out.y = positions[face][vertex][1];
                out.z = positions[face][vertex][2];
                out.abgr = abgr(shades[face], shades[face], shades[face]);
            }
        }
        return result;
    }();
    return vertices;
}

const std::array<u16, 36>& cubeIndices() {
    static const std::array<u16, 36> indices = {{
        0, 1, 2, 0, 2, 3,        // +X
        4, 5, 6, 4, 6, 7,        // -X
        8, 9, 10, 8, 10, 11,     // +Y
        12, 13, 14, 12, 14, 15,  // -Y
        16, 17, 18, 16, 18, 19,  // +Z
        20, 21, 22, 20, 22, 23,  // -Z
    }};
    return indices;
}

// Ground grid on the XZ plane, drawn as a line list. The two world axes are
// tinted so beginners can see which way is which.
std::vector<PosColorVertex> buildGrid(f32 halfExtent, f32 spacing) {
    std::vector<PosColorVertex> vertices;
    const int lines = static_cast<int>(halfExtent / spacing);
    vertices.reserve(static_cast<std::size_t>(lines * 2 + 1) * 4);

    for (int i = -lines; i <= lines; ++i) {
        const f32 offset = static_cast<f32>(i) * spacing;
        const bool isAxis = i == 0;

        // Lines running along Z.
        const u32 alongZ = isAxis ? abgr(0.85f, 0.30f, 0.30f)   // X axis is red
                                  : abgr(0.28f, 0.29f, 0.31f);
        vertices.push_back({offset, 0.0f, -halfExtent, alongZ});
        vertices.push_back({offset, 0.0f, halfExtent, alongZ});

        // Lines running along X.
        const u32 alongX = isAxis ? abgr(0.30f, 0.45f, 0.90f)   // Z axis is blue
                                  : abgr(0.28f, 0.29f, 0.31f);
        vertices.push_back({-halfExtent, 0.0f, offset, alongX});
        vertices.push_back({halfExtent, 0.0f, offset, alongX});
    }
    return vertices;
}

bgfx::RendererType::Enum parseRendererName(const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (const char c : name) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower.empty() || lower == "auto") return bgfx::RendererType::Count;
    if (lower == "vulkan") return bgfx::RendererType::Vulkan;
    if (lower == "opengl" || lower == "gl") return bgfx::RendererType::OpenGL;
    if (lower == "opengles" || lower == "gles") return bgfx::RendererType::OpenGLES;
    if (lower == "d3d11") return bgfx::RendererType::Direct3D11;
    if (lower == "d3d12") return bgfx::RendererType::Direct3D12;
    if (lower == "metal") return bgfx::RendererType::Metal;
    Log::warn("Unknown WAYPOINT_RENDERER '{}'; letting bgfx choose", name);
    return bgfx::RendererType::Count;
}

// bgfx writes shader bytes into this binary folder layout.
const char* shaderFolderFor(bgfx::RendererType::Enum type) {
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "dxil";
        case bgfx::RendererType::Metal: return "metal";
        case bgfx::RendererType::OpenGL: return "glsl";
        case bgfx::RendererType::OpenGLES: return "essl";
        case bgfx::RendererType::Vulkan: return "spirv";
        default: return "spirv";
    }
}

// bgfx talks to the app through this interface: fatal errors, engine traces,
// profiler markers, the shader cache and screenshots.
class RendererCallback final : public bgfx::CallbackI {
public:
    void fatal(const char* filePath, std::uint16_t line, bgfx::Fatal::Enum code,
               const char* message) override {
        Log::critical("[bgfx] fatal ({}) {}:{} {}", static_cast<int>(code),
                      filePath != nullptr ? filePath : "?", line,
                      message != nullptr ? message : "");
    }

    void traceVargs(const char* filePath, std::uint16_t line, const char* format,
                    va_list argList) override {
        char buffer[1024];
        std::vsnprintf(buffer, sizeof(buffer), format, argList);
        Log::debug("[bgfx] {}:{} {}", filePath != nullptr ? filePath : "?", line, buffer);
    }

    void profilerBegin(const char* /*name*/, std::uint32_t /*abgr*/, const char* /*filePath*/,
                       std::uint16_t /*line*/) override {}
    void profilerBeginLiteral(const char* /*name*/, std::uint32_t /*abgr*/,
                              const char* /*filePath*/, std::uint16_t /*line*/) override {}
    void profilerEnd() override {}

    std::uint32_t cacheReadSize(std::uint64_t /*id*/) override { return 0; }
    bool cacheRead(std::uint64_t /*id*/, void* /*data*/, std::uint32_t /*size*/) override {
        return false;
    }
    void cacheWrite(std::uint64_t /*id*/, const void* /*data*/, std::uint32_t /*size*/) override {}

    // Screenshots arrive as tightly packed RGBA; write a PPM, which every image
    // viewer understands and which needs no extra dependency.
    void screenShot(const char* filePath, std::uint32_t width, std::uint32_t height,
                    std::uint32_t pitch, bgfx::TextureFormat::Enum /*format*/, const void* data,
                    std::uint32_t /*size*/, bool yflip) override {
        if (filePath == nullptr || data == nullptr || width == 0 || height == 0) {
            return;
        }
        const auto* pixels = static_cast<const unsigned char*>(data);
        std::FILE* file = std::fopen(filePath, "wb");
        if (file == nullptr) {
            Log::warn("Could not open screenshot path {}", filePath);
            return;
        }
        std::fprintf(file, "P6\n%u %u\n255\n", width, height);
        std::vector<unsigned char> row(width * 3);
        for (std::uint32_t y = 0; y < height; ++y) {
            const std::uint32_t sourceY = yflip ? (height - 1 - y) : y;
            const unsigned char* source = pixels + sourceY * pitch;
            for (std::uint32_t x = 0; x < width; ++x) {
                row[x * 3 + 0] = source[x * 4 + 0];
                row[x * 3 + 1] = source[x * 4 + 1];
                row[x * 3 + 2] = source[x * 4 + 2];
            }
            std::fwrite(row.data(), 1, row.size(), file);
        }
        std::fclose(file);
        Log::info("Screenshot written to {}", filePath);
    }

    void captureBegin(std::uint32_t /*width*/, std::uint32_t /*height*/,
                      std::uint32_t /*pitch*/, bgfx::TextureFormat::Enum /*format*/,
                      bool /*yflip*/) override {}
    void captureEnd() override {}
    void captureFrame(const void* /*data*/, std::uint32_t /*size*/) override {}
};

}  // namespace

struct BgfxRenderer::Impl {
    RendererCallback callback;
    bgfx::VertexLayout layout;
    bgfx::ProgramHandle program{BGFX_INVALID_HANDLE};
    bgfx::UniformHandle tint{BGFX_INVALID_HANDLE};
    bgfx::VertexBufferHandle cubeVertexBuffer{BGFX_INVALID_HANDLE};
    bgfx::IndexBufferHandle cubeIndexBuffer{BGFX_INVALID_HANDLE};
    bgfx::VertexBufferHandle gridVertexBuffer{BGFX_INVALID_HANDLE};
    bgfx::SwapChain swapChain{};
    u16 width = 1280;
    u16 height = 720;
    u32 resetFlags = BGFX_RESET_VSYNC;
    bool initialized = false;
    bool shadersLoaded = false;
    std::string backendName = "none";

    [[nodiscard]] bgfx::ShaderHandle loadShader(const std::string& directory,
                                               const char* profileFolder, const char* name) const {
        const std::filesystem::path path =
            std::filesystem::path(directory) / profileFolder / name;
        const auto bytes = FileSystem::readBinaryFile(path);
        if (!bytes.has_value()) {
            return BGFX_INVALID_HANDLE;
        }
        const bgfx::Memory* memory = bgfx::copy(bytes->data(), static_cast<u32>(bytes->size()));
        return bgfx::createShader(memory);
    }

    void createGeometry() {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();

        const auto& vertices = cubeVertices();
        const auto& indices = cubeIndices();
        cubeVertexBuffer = bgfx::createVertexBuffer(
            bgfx::copy(vertices.data(), static_cast<u32>(sizeof(PosColorVertex) * vertices.size())),
            layout);
        cubeIndexBuffer = bgfx::createIndexBuffer(
            bgfx::copy(indices.data(), static_cast<u32>(sizeof(u16) * indices.size())));

        const std::vector<PosColorVertex> grid = buildGrid(20.0f, 1.0f);
        gridVertexBuffer = bgfx::createVertexBuffer(
            bgfx::copy(grid.data(), static_cast<u32>(sizeof(PosColorVertex) * grid.size())), layout);
    }

    bool createShaders(const std::string& directory) {
        if (directory.empty()) {
            Log::warn("No shader directory was given; the cube will not be drawn");
            return false;
        }

        const bgfx::RendererType::Enum type = bgfx::getRendererType();
        const char* folder = shaderFolderFor(type);

        bgfx::ShaderHandle vertexShader = loadShader(directory, folder, "vs_basic.bin");
        bgfx::ShaderHandle fragmentShader = loadShader(directory, folder, "fs_basic.bin");
        if (!bgfx::isValid(vertexShader) || !bgfx::isValid(fragmentShader)) {
            Log::warn("Shaders not found in {}/{}. Build them with WAYPOINT_BUILD_BGFX_TOOLS=ON.",
                      directory, folder);
            if (bgfx::isValid(vertexShader)) {
                bgfx::destroy(vertexShader);
            }
            if (bgfx::isValid(fragmentShader)) {
                bgfx::destroy(fragmentShader);
            }
            return false;
        }

        program = bgfx::createProgram(vertexShader, fragmentShader, true);
        if (!bgfx::isValid(program)) {
            Log::error("Could not create the shader program");
            return false;
        }

        tint = bgfx::createUniform("u_tint", bgfx::UniformType::Vec4);
        return true;
    }

    // Shown when shaders are missing: a readable message instead of a black screen.
    void drawMissingShaderMessage() const {
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(2, 2, 0x4f, "Waypoint Engine: shaders not found.");
        bgfx::dbgTextPrintf(2, 3, 0x0f, "Expected <install>/shaders/<profile>/vs_basic.bin");
        bgfx::dbgTextPrintf(2, 4, 0x0f, "Build them with WAYPOINT_BUILD_BGFX_TOOLS=ON and rebuild.");
        bgfx::dbgTextPrintf(2, 5, 0x0f, "Backend in use: %s", backendName.c_str());
    }
};

BgfxRenderer::BgfxRenderer() : m_impl(std::make_unique<Impl>()) {}

BgfxRenderer::~BgfxRenderer() {
    shutdown();
}

BgfxRenderer::BgfxRenderer(BgfxRenderer&&) noexcept = default;
BgfxRenderer& BgfxRenderer::operator=(BgfxRenderer&&) noexcept = default;

bool BgfxRenderer::initialize(const RendererConfig& config, std::string* error) {
    if (m_impl->initialized) {
        return true;
    }

    std::string preferred = config.preferredRenderer;
    if (const auto environment = FileSystem::environmentVariable("WAYPOINT_RENDERER");
        environment.has_value()) {
        preferred = *environment;
        Log::info("Renderer forced by WAYPOINT_RENDERER={}", preferred);
    }

    bgfx::Init init;
    init.type = parseRendererName(preferred);
    init.platformData.type = config.nativeWindowKind == NativeWindowKind::Wayland
                                 ? bgfx::NativeWindowHandleType::Wayland
                                 : bgfx::NativeWindowHandleType::Default;
    init.swapChain.nwh = config.nativeWindow;
    init.swapChain.ndt = config.nativeDisplay;
    init.swapChain.width = config.width;
    init.swapChain.height = config.height;
    init.reset = config.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
    init.debug = config.debug;
    init.profile = false;
    init.fallback = true;
    init.callback = &m_impl->callback;
    init.allocator = nullptr;

    m_impl->width = config.width;
    m_impl->height = config.height;
    m_impl->swapChain = init.swapChain;
    m_impl->resetFlags = init.reset;

    if (!bgfx::init(init)) {
        const std::string message =
            "bgfx::init failed. Is a GPU driver and window system available? "
            "(On a headless machine, build with WAYPOINT_HEADLESS_BUILD=1.)";
        if (error != nullptr) {
            *error = message;
        }
        Log::error("{}", message);
        return false;
    }

    m_impl->initialized = true;
    m_impl->backendName = bgfx::getRendererName(bgfx::getRendererType());
    Log::info("Renderer ready: {} {}x{}", m_impl->backendName, config.width, config.height);

    m_impl->createGeometry();
    m_impl->shadersLoaded = m_impl->createShaders(config.shaderDirectory);
    if (!m_impl->shadersLoaded) {
        bgfx::setDebug(BGFX_DEBUG_TEXT);
    }
    return true;
}

void BgfxRenderer::shutdown() {
    if (!m_impl || !m_impl->initialized) {
        return;
    }
    if (bgfx::isValid(m_impl->program)) {
        bgfx::destroy(m_impl->program);
        m_impl->program = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_impl->tint)) {
        bgfx::destroy(m_impl->tint);
        m_impl->tint = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_impl->cubeVertexBuffer)) {
        bgfx::destroy(m_impl->cubeVertexBuffer);
    }
    if (bgfx::isValid(m_impl->cubeIndexBuffer)) {
        bgfx::destroy(m_impl->cubeIndexBuffer);
    }
    if (bgfx::isValid(m_impl->gridVertexBuffer)) {
        bgfx::destroy(m_impl->gridVertexBuffer);
    }
    m_impl->cubeVertexBuffer = BGFX_INVALID_HANDLE;
    m_impl->cubeIndexBuffer = BGFX_INVALID_HANDLE;
    m_impl->gridVertexBuffer = BGFX_INVALID_HANDLE;

    bgfx::shutdown();
    m_impl->initialized = false;
    m_impl->shadersLoaded = false;
}

bool BgfxRenderer::initialized() const {
    return m_impl && m_impl->initialized;
}

void BgfxRenderer::resize(u16 width, u16 height) {
    if (!initialized() || width == 0 || height == 0) {
        return;
    }
    m_impl->width = width;
    m_impl->height = height;
    // The swap chain description is what bgfx resets to; the window itself is
    // already resized by SDL.
    m_impl->swapChain.width = width;
    m_impl->swapChain.height = height;
    bgfx::reset(m_impl->resetFlags, &m_impl->swapChain);
}

void BgfxRenderer::beginFrame(const Transform& cameraTransform, f32 fieldOfViewDegrees) {
    if (!initialized()) {
        return;
    }

    const u16 width = m_impl->width;
    const u16 height = m_impl->height;
    const f32 aspect = height > 0 ? static_cast<f32>(width) / static_cast<f32>(height) : 1.0f;

    bgfx::setViewRect(0, 0, 0, width, height);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1b1d21ff, 1.0f, 0);

    const bgfx::Caps* caps = bgfx::getCaps();
    const f32 nearClip = 0.1f;
    const f32 farClip = 1000.0f;
    // Vulkan/D3D use [0,1] clip depth; OpenGL and Metal use [-1,1].
    const Mat4 projection = caps->homogeneousDepth
                                ? glm::perspectiveRH_NO(glm::radians(fieldOfViewDegrees), aspect,
                                                        nearClip, farClip)
                                : glm::perspectiveRH_ZO(glm::radians(fieldOfViewDegrees), aspect,
                                                        nearClip, farClip);

    // Cameras look down their local -Z axis (see FlyCameraSystem).
    const Vec3 forward = cameraTransform.rotation * Vec3(0.0f, 0.0f, -1.0f);
    const Vec3 up = cameraTransform.rotation * Vec3(0.0f, 1.0f, 0.0f);
    const Mat4 view =
        glm::lookAtRH(cameraTransform.position, cameraTransform.position + forward, up);

    bgfx::setViewTransform(0, glm::value_ptr(view), glm::value_ptr(projection));

    if (!m_impl->shadersLoaded) {
        m_impl->drawMissingShaderMessage();
        return;
    }

    // Ground grid: a line list with no lighting and no depth writes.
    const std::uint64_t gridState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                          BGFX_STATE_PT_LINES | BGFX_STATE_BLEND_ALPHA;
    bgfx::setState(gridState);
    const Mat4 identity(1.0f);
    bgfx::setTransform(glm::value_ptr(identity));
    const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    bgfx::setUniform(m_impl->tint, white);
    bgfx::setVertexBuffer(0, m_impl->gridVertexBuffer);
    bgfx::submit(0, m_impl->program);
}

std::size_t BgfxRenderer::drawWorld(const World& world) {
    if (!initialized() || !m_impl->shadersLoaded) {
        return 0;
    }

    const std::uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                      BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;
    bgfx::setState(state);

    std::size_t drawn = 0;
    const auto view = world.registry().view<const TransformComponent, const MeshRendererComponent>();
    for (const entt::entity entity : view) {
        const auto& mesh = view.get<const MeshRendererComponent>(entity);
        const auto& transform = view.get<const TransformComponent>(entity).transform;

        // Only the built-in cube exists so far; unknown names are reported once
        // per frame at debug level instead of silently drawing nothing.
        if (mesh.mesh != "builtin:cube" && !mesh.mesh.empty()) {
            Log::debug("Mesh '{}' is not available yet; drawing a cube instead", mesh.mesh);
        }

        const Mat4 model = transform.matrix();
        bgfx::setTransform(glm::value_ptr(model));
        const float rgba[4] = {mesh.color.x, mesh.color.y, mesh.color.z, 1.0f};
        bgfx::setUniform(m_impl->tint, rgba);
        bgfx::setVertexBuffer(0, m_impl->cubeVertexBuffer);
        bgfx::setIndexBuffer(m_impl->cubeIndexBuffer);
        bgfx::submit(0, m_impl->program);
        ++drawn;
    }
    return drawn;
}

void BgfxRenderer::endFrame() {
    if (!initialized()) {
        return;
    }
    bgfx::frame();
}

u16 BgfxRenderer::width() const {
    return m_impl ? m_impl->width : 0;
}

u16 BgfxRenderer::height() const {
    return m_impl ? m_impl->height : 0;
}

std::string BgfxRenderer::backendName() const {
    return m_impl ? m_impl->backendName : "none";
}

bool BgfxRenderer::shadersLoaded() const {
    return m_impl && m_impl->shadersLoaded;
}

void BgfxRenderer::requestScreenshot(const std::string& path) {
    if (!initialized()) {
        return;
    }
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
}

}  // namespace waypoint
