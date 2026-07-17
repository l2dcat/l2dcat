#include "bongo/file.h"
#include "bongo/model.h"
#include "cubism_model.hpp"

#include <CubismFramework.hpp>
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
int runtime_count;

void log_message(const char *message) {
    if (message) std::fprintf(stderr, "[Cubism] %s\n", message);
}

Csm::csmByte *load_file(const std::string path, Csm::csmSizeInt *size) {
    if (size) *size = 0;
    FILE *file = bongo_file_open(path.c_str(), "rb");
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

bool start_framework(BongoError *error) {
    if (runtime_count++) return true;
    CubismFramework::Option option{};
    option.LogFunction = log_message;
    option.LoggingLevel = CubismFramework::Option::LogLevel_Warning;
    option.LoadFileFunction = load_file;
    option.ReleaseBytesFunction = release_file;
    if (!CubismFramework::StartUp(&allocator, &option)) {
        runtime_count = 0;
        bongo_error_set(error, BONGO_ERROR_CUBISM, "Cubism Framework startup failed");
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

struct BongoLive2D { bongo::NativeModel *model; };

extern "C" BongoLive2D *bongo_live2d_create(BongoError *error) {
    if (!start_framework(error)) return nullptr;
    BongoLive2D *runtime = new(std::nothrow) BongoLive2D{};
    if (!runtime) {
        stop_framework();
        bongo_error_set(error, BONGO_ERROR_MEMORY, "Cannot allocate Cubism runtime");
    }
    return runtime;
}

extern "C" void bongo_live2d_destroy(BongoLive2D *runtime) {
    if (!runtime) return;
    delete runtime->model;
    delete runtime;
    stop_framework();
}

extern "C" BongoResult bongo_live2d_load(BongoLive2D *runtime, const char *directory,
    const char *setting, BongoError *error) {
    if (!runtime) return BONGO_ERROR_ARGUMENT;
    auto *model = new(std::nothrow) bongo::NativeModel();
    if (!model) return BONGO_ERROR_MEMORY;
    if (!model->load(directory, setting, error)) {
        delete model;
        return error ? error->code : BONGO_ERROR_CUBISM;
    }
    delete runtime->model;
    runtime->model = model;
    return BONGO_OK;
}

extern "C" void bongo_live2d_resize(BongoLive2D *runtime, int width, int height) {
    if (runtime && runtime->model) runtime->model->resize(width, height);
}
extern "C" bool bongo_live2d_update(BongoLive2D *runtime, float elapsed) {
    return runtime && runtime->model && runtime->model->update(elapsed);
}
extern "C" void bongo_live2d_draw(BongoLive2D *runtime) {
    if (runtime && runtime->model) runtime->model->draw();
}
extern "C" void bongo_live2d_set_mirror(BongoLive2D *runtime, bool mirror) {
    if (runtime && runtime->model) runtime->model->set_mirror(mirror);
}
extern "C" bool bongo_live2d_set_parameter(BongoLive2D *runtime, const char *id, float value) {
    return runtime && runtime->model && runtime->model->set_parameter(id, value);
}
extern "C" bool bongo_live2d_parameter(BongoLive2D *runtime, const char *id,
    BongoParameterRange *range) {
    return runtime && runtime->model && range && runtime->model->parameter(id,
        &range->minimum, &range->maximum, &range->value);
}
extern "C" bool bongo_live2d_start_motion(BongoLive2D *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->start_motion(group, index);
}
extern "C" bool bongo_live2d_set_expression(BongoLive2D *runtime, int index) {
    return runtime && runtime->model && runtime->model->set_expression(index);
}
