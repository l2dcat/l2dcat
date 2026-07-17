#ifndef BONGO_CUBISM_MODEL_HPP
#define BONGO_CUBISM_MODEL_HPP

#include "bongo/common.h"

#include <Model/CubismUserModel.hpp>
#include <CubismModelSettingJson.hpp>
#include <Motion/ACubismMotion.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <SDL3/SDL_opengl.h>
#include <map>
#include <string>
#include <vector>

namespace bongo {

class NativeModel final : public Csm::CubismUserModel {
public:
    NativeModel();
    ~NativeModel() override;
    bool load(const char *directory, const char *setting_file, BongoError *error);
    void resize(int width, int height);
    bool update(float delta_seconds);
    void draw();
    void set_mirror(bool mirror);
    bool set_parameter(const char *id, float value);
    bool parameter(const char *id, float *minimum, float *maximum, float *value);
    bool start_motion(const char *group, int index);
    bool set_expression(int index);

private:
    using MotionMap = std::map<std::string, Csm::ACubismMotion *>;
    bool load_model(BongoError *error);
    void load_expressions();
    void load_effects();
    void load_motions();
    bool load_textures(BongoError *error);
    void bind_textures();
    std::vector<unsigned char> read(const std::string &path) const;
    std::string path(const char *relative) const;

    Csm::CubismModelSettingJson *setting_ = nullptr;
    MotionMap motions_;
    MotionMap expressions_;
    std::vector<std::string> expression_names_;
    std::vector<GLuint> textures_;
    std::vector<float> parameter_snapshot_;
    std::vector<float> part_snapshot_;
    Csm::csmVector<Csm::CubismIdHandle> eye_blink_ids_;
    Csm::csmVector<Csm::CubismIdHandle> lip_sync_ids_;
    std::string directory_;
    int width_ = 612;
    int height_ = 354;
    bool motion_updated_ = false;
    bool mirror_ = false;
    float opacity_snapshot_ = -1.0f;
};

} // namespace bongo

#endif
