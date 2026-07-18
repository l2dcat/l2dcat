#include "l2dcat/file.h"
#include "l2dcat/model.h"
#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL)
#include <GL/glew.h>
#endif
#include "cubism_model.hpp"

#include <CubismFramework.hpp>
#include <SDL3/SDL_filesystem.h>
#include <cstdio>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif

using Csm::CubismFramework;

namespace {

class Allocator final : public Csm::ICubismAllocator {
public:
    void *Allocate(const Csm::csmSizeType size) override { return std::malloc(size); }
    void Deallocate(void *memory) override { std::free(memory); }
    void *AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment) override {
#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        void *memory = nullptr;
        return posix_memalign(&memory, alignment, size) == 0 ? memory : nullptr;
#endif
    }
    void DeallocateAligned(void *memory) override {
#ifdef _WIN32
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
};

Allocator allocator;
CubismFramework::Option framework_option;
std::string resource_root;
int runtime_count;

void log_message(const char *message) {
    if (message) std::fprintf(stderr, "[Cubism] %s\n", message);
}

Csm::csmByte *load_file(const std::string path, Csm::csmSizeInt *size) {
    if (size) *size = 0;
    FILE *file = l2dcat_file_open(path.c_str(), "rb");
    if (!file) {
        const char *base = SDL_GetBasePath();
        if (base) file = l2dcat_file_open((std::string(base) + path).c_str(), "rb");
    }
    if (!file && !resource_root.empty())
        file = l2dcat_file_open((resource_root + "/" + path).c_str(), "rb");
    if (!file) return nullptr;
    std::fseek(file, 0, SEEK_END);
    long length = std::ftell(file);
    std::rewind(file);
    if (length <= 0) { std::fclose(file); return nullptr; }
    auto *bytes = static_cast<Csm::csmByte *>(std::malloc((size_t)length));
    if (!bytes || std::fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        std::free(bytes);
        bytes = nullptr;
    } else if (size) *size = (Csm::csmSizeInt)length;
    std::fclose(file);
    return bytes;
}

void release_file(Csm::csmByte *bytes) { std::free(bytes); }

bool start_framework(L2DCatError *error) {
    if (runtime_count++) return true;
#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL)
    glewExperimental = GL_TRUE;
    GLenum glew_result = glewInit();
    glGetError();
    if (glew_result != GLEW_OK) {
        runtime_count = 0;
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "GLEW initialization failed: %s",
            reinterpret_cast<const char *>(glewGetErrorString(glew_result)));
        return false;
    }
    if (!glCreateShader || !glShaderSource || !glCompileShader ||
        !glGetShaderiv || !glCreateProgram || !glGenFramebuffers) {
        runtime_count = 0;
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM,
            "Required OpenGL 3.3 functions are unavailable");
        return false;
    }
#endif
    framework_option = CubismFramework::Option{};
    framework_option.LogFunction = log_message;
    framework_option.LoggingLevel = CubismFramework::Option::LogLevel_Warning;
    framework_option.LoadFileFunction = load_file;
    framework_option.ReleaseBytesFunction = release_file;
    if (!CubismFramework::StartUp(&allocator, &framework_option)) {
        runtime_count = 0;
        l2dcat_error_set(error, L2DCAT_ERROR_CUBISM, "Cubism Framework startup failed");
        return false;
    }
    CubismFramework::Initialize();
    return true;
}

void stop_framework() {
    if (--runtime_count > 0) return;
    CubismFramework::Dispose();
    CubismFramework::CleanUp();
    runtime_count = 0;
}

} // namespace

struct L2DCatLive2D { l2dcat::NativeModel *model; };

extern "C" L2DCatLive2D *l2dcat_live2d_create(const char *asset_root,
    L2DCatError *error) {
    resource_root = asset_root ? asset_root : "";
    if (!start_framework(error)) return nullptr;
    L2DCatLive2D *runtime = new(std::nothrow) L2DCatLive2D{};
    if (!runtime) {
        stop_framework();
        l2dcat_error_set(error, L2DCAT_ERROR_MEMORY, "Cannot allocate Cubism runtime");
    }
    return runtime;
}

extern "C" void l2dcat_live2d_destroy(L2DCatLive2D *runtime) {
    if (!runtime) return;
    delete runtime->model;
    delete runtime;
    stop_framework();
}

extern "C" L2DCatResult l2dcat_live2d_load(L2DCatLive2D *runtime, const char *directory,
    const char *setting, L2DCatError *error) {
    if (!runtime) return L2DCAT_ERROR_ARGUMENT;
    auto *model = new(std::nothrow) l2dcat::NativeModel();
    if (!model) return L2DCAT_ERROR_MEMORY;
    if (!model->load(directory, setting, error)) {
        delete model;
        return error ? error->code : L2DCAT_ERROR_CUBISM;
    }
    delete runtime->model;
    runtime->model = model;
    return L2DCAT_OK;
}

extern "C" void l2dcat_live2d_resize(L2DCatLive2D *runtime, int width, int height) {
    if (runtime && runtime->model) runtime->model->resize(width, height);
}
extern "C" bool l2dcat_live2d_update(L2DCatLive2D *runtime, float elapsed) {
    return runtime && runtime->model && runtime->model->update(elapsed);
}
extern "C" void l2dcat_live2d_draw(L2DCatLive2D *runtime) {
    if (runtime && runtime->model) runtime->model->draw();
}
extern "C" void l2dcat_live2d_set_mirror(L2DCatLive2D *runtime, bool mirror) {
    if (runtime && runtime->model) runtime->model->set_mirror(mirror);
}
extern "C" bool l2dcat_live2d_set_parameter(L2DCatLive2D *runtime, const char *id, float value) {
    return runtime && runtime->model && runtime->model->set_parameter(id, value);
}
extern "C" bool l2dcat_live2d_parameter(L2DCatLive2D *runtime, const char *id,
    L2DCatParameterRange *range) {
    return runtime && runtime->model && range && runtime->model->parameter(id,
        &range->minimum, &range->maximum, &range->value);
}
extern "C" bool l2dcat_live2d_start_motion(L2DCatLive2D *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->start_motion(group, index);
}
extern "C" bool l2dcat_live2d_set_expression(L2DCatLive2D *runtime, int index) {
    return runtime && runtime->model && runtime->model->set_expression(index);
}
