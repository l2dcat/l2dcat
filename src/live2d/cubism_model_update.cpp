#include "cubism_model.hpp"

#include <Id/CubismIdManager.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Model/CubismModel.hpp>
#include <Motion/CubismExpressionMotionManager.hpp>
#include <Motion/CubismMotionManager.hpp>
#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>
#include <cmath>

namespace l2dcat {

void NativeModel::resize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) return;
    width_ = width;
    height_ = height;
    if (!_model) return;
    DeleteRenderer();
    CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_);
    bind_textures();
}

template<typename Getter>
static bool changed(std::vector<float> &snapshot, int count, Getter value) {
    bool result = snapshot.size() != (size_t)count;
    if (result) snapshot.resize((size_t)count);
    for (int i = 0; i < count; ++i) {
        float current = value(i);
        if (result || std::fabs(snapshot[(size_t)i] - current) > 0.00001f) result = true;
        snapshot[(size_t)i] = current;
    }
    return result;
}

bool NativeModel::update(float delta_seconds) {
    if (!_model) return false;
    accumulated_seconds_ += delta_seconds;
    bool active = external_parameters_dirty_ || !_motionManager->IsFinished() ||
        !_expressionManager->IsFinished();
    if (!active && accumulated_seconds_ < 0.03f) return false;
    delta_seconds = accumulated_seconds_;
    accumulated_seconds_ = 0.0f;
    external_parameters_dirty_ = false;
    motion_updated_ = false;
    _model->LoadParameters();
    if (!_motionManager->IsFinished())
        motion_updated_ = _motionManager->UpdateMotion(_model, delta_seconds);
    _model->SaveParameters();
    _updateScheduler.OnLateUpdate(_model, delta_seconds);
    _opacity = _model->GetModelOpacity();
    _model->Update();
    bool result = changed(parameter_snapshot_, _model->GetParameterCount(),
        [this](int index) { return _model->GetParameterValue(index); });
    result = changed(part_snapshot_, _model->GetPartCount(),
        [this](int index) { return _model->GetPartOpacity(index); }) || result;
    if (std::fabs(opacity_snapshot_ - _opacity) > 0.00001f) result = true;
    opacity_snapshot_ = _opacity;
    return result;
}

void NativeModel::draw() {
    if (!_model || width_ <= 0 || height_ <= 0) return;
    auto *manager = Csm::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance();
    manager->BeginFrameProcess();
    Csm::CubismMatrix44 projection;
    if (_model->GetCanvasWidth() > 1.0f && width_ < height_) {
        _modelMatrix->SetWidth(2.0f);
        projection.Scale(mirror_ ? -1.0f : 1.0f, (float)width_ / (float)height_);
    } else {
        projection.Scale((mirror_ ? -1.0f : 1.0f) * (float)height_ / (float)width_, 1.0f);
    }
    projection.MultiplyByMatrix(_modelMatrix);
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    renderer->SetMvpMatrix(&projection);
    renderer->DrawModel();
    manager->EndFrameProcess();
}

void NativeModel::set_mirror(bool mirror) { mirror_ = mirror; }

bool NativeModel::set_parameter(const char *id, float value) {
    if (!_model || !id) return false;
    Csm::CubismIdHandle handle = Csm::CubismFramework::GetIdManager()->GetId(id);
    int index = _model->GetParameterIndex(handle);
    if (index < 0 || index >= _model->GetParameterCount()) return false;
    _model->SetParameterValue(index, value);
    _model->SaveParameters();
    external_parameters_dirty_ = true;
    return true;
}

bool NativeModel::parameter(const char *id, float *minimum, float *maximum, float *value) {
    if (!_model || !id) return false;
    Csm::CubismIdHandle handle = Csm::CubismFramework::GetIdManager()->GetId(id);
    int index = _model->GetParameterIndex(handle);
    if (index < 0 || index >= _model->GetParameterCount()) return false;
    if (minimum) *minimum = _model->GetParameterMinimumValue(index);
    if (maximum) *maximum = _model->GetParameterMaximumValue(index);
    if (value) *value = _model->GetParameterValue(index);
    return true;
}

bool NativeModel::start_motion(const char *group, int index) {
    if (!group || index < 0) return false;
    std::string key = std::string(group) + "_" + std::to_string(index);
    auto found = motions_.find(key);
    if (found == motions_.end()) return false;
    constexpr int priority = 2;
    if (!_motionManager->ReserveMotion(priority)) return false;
    _motionManager->StartMotionPriority(found->second, false, priority);
    return true;
}

bool NativeModel::set_expression(int index) {
    if (index < 0 || (size_t)index >= expression_names_.size()) return false;
    auto found = expressions_.find(expression_names_[(size_t)index]);
    if (found == expressions_.end()) return false;
    _expressionManager->StartMotion(found->second, false);
    return true;
}

} // namespace l2dcat
