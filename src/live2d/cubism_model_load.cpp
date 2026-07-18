#include "cubism_model.hpp"
#include "l2dcat/file.h"
#include "l2dcat/image.h"

#include <Effect/CubismEyeBlink.hpp>
#include <Motion/CubismExpressionUpdater.hpp>
#include <Motion/CubismEyeBlinkUpdater.hpp>
#include <Motion/CubismMotion.hpp>
#include <Motion/CubismPhysicsUpdater.hpp>
#include <Motion/CubismPoseUpdater.hpp>
#include <Physics/CubismPhysics.hpp>
#include <cstdio>
#include <new>

namespace l2dcat {

NativeModel::NativeModel() {
    _mocConsistency = true;
    _motionConsistency = true;
}

NativeModel::~NativeModel() {
    for (auto &item : motions_) Csm::ACubismMotion::Delete(item.second);
    for (auto &item : expressions_) Csm::ACubismMotion::Delete(item.second);
    if (!textures_.empty()) glDeleteTextures((GLsizei)textures_.size(), textures_.data());
    delete setting_;
}

std::vector<unsigned char> NativeModel::read(const std::string &file) const {
    FILE *stream = l2dcat_file_open(file.c_str(), "rb");
    if (!stream || std::fseek(stream, 0, SEEK_END) != 0) {
        if (stream) std::fclose(stream);
        return {};
    }
    long size = std::ftell(stream);
    if (size <= 0 || std::fseek(stream, 0, SEEK_SET) != 0) {
        std::fclose(stream);
        return {};
    }
    std::vector<unsigned char> bytes((size_t)size);
    bool read = std::fread(bytes.data(), 1, (size_t)size, stream) == (size_t)size;
    std::fclose(stream);
    if (!read) return {};
    return bytes;
}

std::string NativeModel::path(const char *relative) const {
    return directory_ + (relative ? relative : "");
}

bool NativeModel::load(const char *directory, const char *setting_file, L2DCatError *error) {
    if (!directory || !setting_file) return false;
    directory_ = directory;
    if (!directory_.empty() && directory_.back() != '/' && directory_.back() != '\\')
        directory_ += '/';
    std::vector<unsigned char> json = read(path(setting_file));
    if (json.empty()) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot read model setting: %s", setting_file);
        return false;
    }
    setting_ = new(std::nothrow)
        Csm::CubismModelSettingJson(json.data(), (Csm::csmSizeInt)json.size());
    if (!setting_) {
        l2dcat_error_set(error, L2DCAT_ERROR_MEMORY, "Cannot allocate model setting");
        return false;
    }
    if (!load_model(error)) return false;
    load_expressions();
    load_effects();
    load_motions();
    Csm::csmMap<Csm::csmString, Csm::csmFloat32> layout;
    setting_->GetLayoutMap(layout);
    _modelMatrix->SetupFromLayout(layout);
    _model->SaveParameters();
    CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_);
    return load_textures(error);
}

bool NativeModel::load_model(L2DCatError *error) {
    const char *name = setting_->GetModelFileName();
    std::vector<unsigned char> bytes = read(path(name));
    if (bytes.empty()) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot read moc3: %s", name);
        return false;
    }
    LoadModel(bytes.data(), (Csm::csmSizeInt)bytes.size(), true);
    if (!_model) {
        l2dcat_error_set(error, L2DCAT_ERROR_CUBISM, "Cubism rejected moc3: %s", name);
        return false;
    }
    return true;
}

void NativeModel::load_expressions() {
    for (int i = 0; i < setting_->GetExpressionCount(); ++i) {
        const char *name = setting_->GetExpressionName(i);
        std::vector<unsigned char> bytes = read(path(setting_->GetExpressionFileName(i)));
        if (bytes.empty()) continue;
        Csm::ACubismMotion *motion = LoadExpression(bytes.data(),
            (Csm::csmSizeInt)bytes.size(), name);
        if (!motion) continue;
        expressions_[name] = motion;
        expression_names_.emplace_back(name);
    }
    if (!expressions_.empty()) {
        auto *updater = CSM_NEW Csm::CubismExpressionUpdater(*_expressionManager);
        _updateScheduler.AddUpdatableList(updater);
    }
}

void NativeModel::load_effects() {
    if (setting_->GetPhysicsFileName()[0]) {
        auto bytes = read(path(setting_->GetPhysicsFileName()));
        if (!bytes.empty()) LoadPhysics(bytes.data(), (Csm::csmSizeInt)bytes.size());
        if (_physics) _updateScheduler.AddUpdatableList(
            CSM_NEW Csm::CubismPhysicsUpdater(*_physics));
    }
    if (setting_->GetPoseFileName()[0]) {
        auto bytes = read(path(setting_->GetPoseFileName()));
        if (!bytes.empty()) LoadPose(bytes.data(), (Csm::csmSizeInt)bytes.size());
        if (_pose) _updateScheduler.AddUpdatableList(CSM_NEW Csm::CubismPoseUpdater(*_pose));
    }
    if (setting_->GetUserDataFile()[0]) {
        auto bytes = read(path(setting_->GetUserDataFile()));
        if (!bytes.empty()) LoadUserData(bytes.data(), (Csm::csmSizeInt)bytes.size());
    }
    for (int i = 0; i < setting_->GetEyeBlinkParameterCount(); ++i)
        eye_blink_ids_.PushBack(setting_->GetEyeBlinkParameterId(i));
    for (int i = 0; i < setting_->GetLipSyncParameterCount(); ++i)
        lip_sync_ids_.PushBack(setting_->GetLipSyncParameterId(i));
    if (setting_->GetEyeBlinkParameterCount() > 0) {
        _eyeBlink = Csm::CubismEyeBlink::Create(setting_);
        _updateScheduler.AddUpdatableList(
            CSM_NEW Csm::CubismEyeBlinkUpdater(motion_updated_, *_eyeBlink));
    }
    _updateScheduler.SortUpdatableList();
}

void NativeModel::load_motions() {
    for (int group_index = 0; group_index < setting_->GetMotionGroupCount(); ++group_index) {
        const char *group = setting_->GetMotionGroupName(group_index);
        for (int i = 0; i < setting_->GetMotionCount(group); ++i) {
            auto bytes = read(path(setting_->GetMotionFileName(group, i)));
            if (bytes.empty()) continue;
            std::string key = std::string(group) + "_" + std::to_string(i);
            auto *motion = static_cast<Csm::CubismMotion *>(LoadMotion(bytes.data(),
                (Csm::csmSizeInt)bytes.size(), key.c_str(), nullptr, nullptr,
                setting_, group, i, _motionConsistency));
            if (!motion) continue;
            motion->SetEffectIds(eye_blink_ids_, lip_sync_ids_);
            motions_[key] = motion;
        }
    }
    _motionManager->StopAllMotions();
}

bool NativeModel::load_textures(L2DCatError *error) {
    textures_.assign((size_t)setting_->GetTextureCount(), 0);
    for (int i = 0; i < setting_->GetTextureCount(); ++i) {
        textures_[(size_t)i] = l2dcat_image_texture(
            path(setting_->GetTextureFileName(i)).c_str(), nullptr, nullptr, error);
        if (!textures_[(size_t)i]) return false;
    }
    bind_textures();
    return true;
}

void NativeModel::bind_textures() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    if (!renderer) return;
    for (size_t i = 0; i < textures_.size(); ++i)
        if (textures_[i]) renderer->BindTexture((Csm::csmInt32)i, textures_[i]);
    renderer->IsPremultipliedAlpha(false);
}

} // namespace l2dcat
