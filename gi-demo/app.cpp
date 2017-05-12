#include "app.h"
#include "debug_draw.h"
#include "vendor/tinyobjloader/tiny_obj_loader.h"
#include <OpenGL/gl3.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vectorial/vectorial.h>

#define GL_CHECK_ENABLED 1

#if GL_CHECK_ENABLED
#define GL_CHECK(expr)                                                                                                 \
  do {                                                                                                                 \
    (expr);                                                                                                            \
    GLenum err = glGetError();                                                                                         \
    if (err != GL_NO_ERROR) {                                                                                          \
      report_error("GL expr failed. expr=`%s` code=%04xh msg=%s\n", #expr, err, get_gl_error_description(err));        \
    }                                                                                                                  \
  } while (false)
#else
#define GL_CHECK(expr) (expr)
#endif

struct Model {
  vectorial::mat4f transform;
  GLuint ib;
  GLuint vb;
  int tri_count;
  bool wireframe;
};

struct Camera {
  vectorial::vec3f pos;
  float pitch;
  float yaw;
  float near;
  float far;
  vectorial::mat4f projection;
};

struct Light {
  vectorial::vec3f pos;
  vectorial::vec3f color;
  float intensity;
  float range;
};

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Vertex {
  Vec3 p;
  Vec3 n;
  Vec3 c;
};

struct VertexPN {
  Vec3 p;
  Vec3 n;
};

static bool s_first_draw = true;
static float s_time = 0.0f;
static std::vector<Model> s_models;
static Camera s_camera;
static Light s_light;

// debug
static bool s_draw_wireframe = false;
static bool s_draw_depth = false;

static GLuint s_default_vao;
static GLuint s_program;
static GLuint s_program_depth;

static bool s_keyStatus[APP_KEY_CODE_COUNT];

static GLuint s_debug_draw_points_vb;
static GLuint s_debug_draw_lines_vb;
static GLuint s_debug_draw_program;
static std::vector<VertexPN> s_debug_normals;

static void report_error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

static const char* get_gl_error_description(GLint err) {
  switch (err) {
    case GL_INVALID_ENUM:
      return "invalid enum";
    case GL_INVALID_VALUE:
      return "invalid value";
    case GL_INVALID_OPERATION:
      return "invalid operation";
    case GL_OUT_OF_MEMORY:
      return "out of memory";
    default:
      return "unknown error code";
  }
}

static void load_file(std::string* out, const char* filename) {
  if (std::ifstream is{filename, std::ios::binary | std::ios::ate}) {
    auto size = is.tellg();
    out->resize(size, '\0');
    is.seekg(0);
    is.read(&(*out)[0], size);
  }
}

static GLuint load_shader(const char* filename_vs, const char* filename_fs) {
  std::string vertex_code;
  std::string fragment_code;
  load_file(&vertex_code, filename_vs);
  load_file(&fragment_code, filename_fs);

  GLint result = GL_FALSE;
  int info_log_length;

  const char* vertex_code_str = vertex_code.c_str();
  const char* fragment_code_str = fragment_code.c_str();

  // compile vertex shader
  GLuint vertex_shader;
  GL_CHECK(vertex_shader = glCreateShader(GL_VERTEX_SHADER));
  GL_CHECK(glShaderSource(vertex_shader, 1, &vertex_code_str, nullptr));
  GL_CHECK(glCompileShader(vertex_shader));
  GL_CHECK(glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result));
  GL_CHECK(glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &info_log_length));
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    GL_CHECK(glGetShaderInfoLog(vertex_shader, info_log_length, nullptr, &err_msg[0]));
    std::cerr << "VERTEX SHADER ERROR: (" << filename_vs << ")" << err_msg << std::endl;
    GL_CHECK(glDeleteShader(vertex_shader));
    return 0;
  }

  // compile fragment shader
  GLuint fragment_shader;
  GL_CHECK(fragment_shader = glCreateShader(GL_FRAGMENT_SHADER));
  GL_CHECK(glShaderSource(fragment_shader, 1, &fragment_code_str, nullptr));
  GL_CHECK(glCompileShader(fragment_shader));
  GL_CHECK(glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result));
  GL_CHECK(glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &info_log_length));
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    GL_CHECK(glGetShaderInfoLog(fragment_shader, info_log_length, nullptr, &err_msg[0]));
    std::cerr << "FRAGMENT SHADER ERROR: (" << filename_vs << ")" << err_msg << std::endl;
    GL_CHECK(glDeleteShader(fragment_shader));
    return 0;
  }

  // link
  GLuint program;
  GL_CHECK(program = glCreateProgram());
  GL_CHECK(glAttachShader(program, vertex_shader));
  GL_CHECK(glAttachShader(program, fragment_shader));
  GL_CHECK(glLinkProgram(program));
  GL_CHECK(glGetProgramiv(program, GL_LINK_STATUS, &result));
  GL_CHECK(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length));
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    GL_CHECK(glGetProgramInfoLog(program, info_log_length, nullptr, &err_msg[0]));
    std::cerr << "SHADER LINK ERROR: (" << filename_vs << "," << filename_fs << ")" << err_msg << std::endl;
    GL_CHECK(glDetachShader(program, vertex_shader));
    GL_CHECK(glDetachShader(program, fragment_shader));
    GL_CHECK(glDeleteShader(vertex_shader));
    GL_CHECK(glDeleteShader(fragment_shader));
    GL_CHECK(glDeleteProgram(program));
    return 0;
  }

  GL_CHECK(glDetachShader(program, vertex_shader));
  GL_CHECK(glDetachShader(program, fragment_shader));
  GL_CHECK(glDeleteShader(vertex_shader));
  GL_CHECK(glDeleteShader(fragment_shader));
  return program;
}

static GLuint load_shader(const char* base_filename) {
  const std::string filename_vs = std::string(base_filename) + ".vs.glsl";
  const std::string filename_fs = std::string(base_filename) + ".fs.glsl";
  return load_shader(filename_vs.c_str(), filename_fs.c_str());
}

static vectorial::vec3f
normal_from_face(const vectorial::vec3f& p0, const vectorial::vec3f& p1, const vectorial::vec3f& p2) {
  vectorial::vec3f p01 = p1 - p0;
  vectorial::vec3f p02 = p2 - p0;
  vectorial::vec3f cross = vectorial::cross(p01, p02);
  vectorial::vec3f normal = vectorial::normalize(cross);
  return normal;
}

static void load_model(const char* filename, const char* mtl_dirname, const vectorial::mat4f& transform) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, mtl_dirname, true);
  if (!err.empty()) {
    std::cerr << "ERROR: " << err << std::endl;
  }
  if (!ret) {
    exit(1);
  }

  Model model;
  model.ib = 0;
  model.vb = 0;
  model.tri_count = 0;
  model.wireframe = false;

  std::vector<uint16_t> indices;
  std::vector<Vertex> vertices;

  for (const tinyobj::shape_t& shape : shapes) {
    for (size_t face = 0, face_count = shape.mesh.indices.size() / 3; face < face_count; ++face) {
      tinyobj::index_t idx0 = shape.mesh.indices[3 * face + 0];
      tinyobj::index_t idx1 = shape.mesh.indices[3 * face + 1];
      tinyobj::index_t idx2 = shape.mesh.indices[3 * face + 2];

      // positions
      vectorial::vec3f pos0, pos1, pos2;
      pos0.load(&attrib.vertices[3 * idx0.vertex_index]);
      pos1.load(&attrib.vertices[3 * idx1.vertex_index]);
      pos2.load(&attrib.vertices[3 * idx2.vertex_index]);
      pos0 = vectorial::transformPoint(transform, pos0);
      pos1 = vectorial::transformPoint(transform, pos1);
      pos2 = vectorial::transformPoint(transform, pos2);

      // normals
      vectorial::vec3f nor0, nor1, nor2;
      if (attrib.normals.size() > 0) {
        nor0.load(&attrib.normals[3 * idx0.normal_index]);
        nor1.load(&attrib.normals[3 * idx1.normal_index]);
        nor2.load(&attrib.normals[3 * idx2.normal_index]);
        nor0 = vectorial::transformVector(transform, nor0);
        nor1 = vectorial::transformVector(transform, nor1);
        nor2 = vectorial::transformVector(transform, nor2);
      }
      else {
        vectorial::vec3f normal = normal_from_face(pos0, pos1, pos2);
        nor0 = normal;
        nor1 = normal;
        nor2 = normal;
      }

      vectorial::vec3f center;
      center = pos0 + pos1 + pos2;
      center /= 3.0f;
      VertexPN vtx;
      center.store(&vtx.p.x);
      nor0.store(&vtx.n.x);
      s_debug_normals.push_back(vtx);

      vectorial::vec3f color;
      if (materials.size() > 0) {
        const tinyobj::material_t& mat = materials[shape.mesh.material_ids[face]];
        color.load(mat.diffuse);
      }
      else {
        color = vectorial::vec3f(0.5f);
      }

      Vertex v0, v1, v2;
      pos0.store(&v0.p.x);
      nor0.store(&v0.n.x);
      color.store(&v0.c.x);
      pos1.store(&v1.p.x);
      nor1.store(&v1.n.x);
      color.store(&v1.c.x);
      pos2.store(&v2.p.x);
      nor2.store(&v2.n.x);
      color.store(&v2.c.x);
      vertices.push_back(v0);
      vertices.push_back(v1);
      vertices.push_back(v2);

      indices.push_back(indices.size());
      indices.push_back(indices.size());
      indices.push_back(indices.size());
    }
  }

  // create the index buffer
  const uint16_t* ib_data = &indices[0];
  int ib_size_bytes = (int)indices.size() * sizeof(uint16_t);
  GL_CHECK(glGenBuffers(1, &model.ib));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib));
  GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, ib_size_bytes, ib_data, GL_STATIC_DRAW));

  const Vertex* vb_data = &vertices[0];
  int vb_size_bytes = (int)vertices.size() * sizeof(Vertex);
  GL_CHECK(glGenBuffers(1, &model.vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model.vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vb_size_bytes, vb_data, GL_STATIC_DRAW));

  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

  // finish up the model and save it
  model.tri_count = (int)indices.size() / 3;
  model.transform = vectorial::mat4f::identity();
  s_models.push_back(model);
}

static void model_destroy(Model* model) {
  GL_CHECK(glDeleteBuffers(1, &model->ib));
  GL_CHECK(glDeleteBuffers(1, &model->vb));
}

static void load_models() {
  // char * dir = getcwd(NULL, 0);
  // std::cout << "Current dir: " << dir << std::endl;

  const char* mtl_dirname = "gi-demo/data/";
  // load_model("gi-demo/data/floor.obj", mtl_dirname);
  load_model("gi-demo/data/cornell_box.obj",
             mtl_dirname,
             vectorial::mat4f::scale(10.0f) *
                 vectorial::mat4f::axisRotation(1.5708f, vectorial::vec3f(1.0f, 0.0f, 0.0f)));
  // s_models.back().transform = vectorial::mat4f::scale(10.0f) * vectorial::mat4f::axisRotation(1.5708f,
  // vectorial::vec3f(1.0f, 0.0f, 0.0f)); s_models.back().transform = vectorial::mat4f::scale(0.1f) *
  // vectorial::mat4f::axisRotation(1.5708f, vectorial::vec3f(1.0f, 0.0f, 0.0f));
}

static void unload_models() {
  for (Model& model : s_models) {
    model_destroy(&model);
  }
  s_models.clear();
}

static void load_shaders() {
  s_program = load_shader("gi-demo/data/shaders/lit");
  s_program_depth = load_shader("gi-demo/data/shaders/lit.vs.glsl", "gi-demo/data/shaders/depth.fs.glsl");
}

static void unload_shaders() {
  GL_CHECK(glDeleteProgram(s_program_depth));
  GL_CHECK(glDeleteProgram(s_program));
  s_program_depth = 0;
  s_program = 0;
}

static void bind_constant_float(GLuint program, const char* name, float value) {
  // TODO: cache the uniform id
  GLuint uniform_id;
  GL_CHECK(uniform_id = glGetUniformLocation(program, name));
  GL_CHECK(glUniform1fv(uniform_id, 1, &value));
}

static void bind_constant_vec2(GLuint program, const char* name, const vectorial::vec2f& value) {
  float value_f[2];
  value.store(value_f);

  // TODO: cache the uniform id
  GLuint uniform_id;
  GL_CHECK(uniform_id = glGetUniformLocation(program, name));
  GL_CHECK(glUniform2fv(uniform_id, 1, value_f));
}

static void bind_constant_vec3(GLuint program, const char* name, const vectorial::vec3f& value) {
  float value_f[3];
  value.store(value_f);

  // TODO: cache the uniform id
  GLuint uniform_id;
  GL_CHECK(uniform_id = glGetUniformLocation(program, name));
  GL_CHECK(glUniform3fv(uniform_id, 1, value_f));
}

static void bind_constant_mat4(GLuint program, const char* name, const vectorial::mat4f& value) {
  float value_f[16];
  value.store(value_f);

  // TODO: cache the uniform id
  GLuint uniform_id;
  GL_CHECK(uniform_id = glGetUniformLocation(program, name));
  GL_CHECK(glUniformMatrix4fv(uniform_id, 1, GL_FALSE, value_f));
}

static void bind_constants(GLuint program,
                           const vectorial::mat4f& world,
                           const vectorial::mat4f& view,
                           const vectorial::mat4f& proj) {
  // add a transform to rotation Z up to Y up
  // NOTE: this is applied to the view transform (inverse of the camera world transform)
  vectorial::mat4f makeYUp = vectorial::mat4f::axisRotation(-1.5708f, vectorial::vec3f(1.0f, 0.0f, 0.0f));
  const vectorial::mat4f world_view = makeYUp * view * world;
  const vectorial::mat4f world_view_proj = proj * world_view;

  const vectorial::vec3f light_pos_vs = vectorial::transformPoint((makeYUp * view), s_light.pos);

  // loop over the program's uniforms
  GLint uniform_count;
  GLint uniform_name_max_len;
  GL_CHECK(glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count));
  GL_CHECK(glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_name_max_len));
  char* uniform_name = (char*)alloca(uniform_name_max_len);
  for (int index = 0; index < uniform_count; ++index) {
    GLint uniform_size;
    GLenum uniform_type;
    GL_CHECK(
        glGetActiveUniform(program, index, uniform_name_max_len, nullptr, &uniform_size, &uniform_type, uniform_name));
    // printf("Uniform[%d]: '%s'\n", index, uniform_name);

    if (0 == strcmp(uniform_name, "world_view_proj")) {
      bind_constant_mat4(program, "world_view_proj", world_view_proj);
    }
    else if (0 == strcmp(uniform_name, "world_view")) {
      bind_constant_mat4(program, "world_view", world_view);
    }
    else if (0 == strcmp(uniform_name, "light_pos_vs")) {
      bind_constant_vec3(program, "light_pos_vs", light_pos_vs);
    }
    else if (0 == strcmp(uniform_name, "light_color")) {
      bind_constant_vec3(program, "light_color", s_light.color);
    }
    else if (0 == strcmp(uniform_name, "light_intensity")) {
      bind_constant_float(program, "light_intensity", s_light.intensity);
    }
    else if (0 == strcmp(uniform_name, "light_range")) {
      bind_constant_float(program, "light_range", s_light.range);
    }
    else if (0 == strcmp(uniform_name, "camera_near_far")) {
      bind_constant_vec2(program, "camera_near_far", vectorial::vec2f(s_camera.near, s_camera.far));
    }
    else if (0 == strncmp(uniform_name, "gl_", 3)) {
      // ignore
    }
    else {
      printf("WARN: Unknown uniform: '%s'\n", uniform_name);
    }
  }
}

static vectorial::mat4f makeCameraTransform(Camera* cam) {
  vectorial::mat4f cameraYaw = vectorial::mat4f::axisRotation(cam->yaw, vectorial::vec3f(0.0f, 0.0f, 1.0f));
  vectorial::mat4f cameraPitch = vectorial::mat4f::axisRotation(cam->pitch, cameraYaw.value.x);
  return vectorial::mat4f::translation(cam->pos) * cameraPitch * cameraYaw;
}

static void debug_draw_points(const DDrawVertex* vertices, int vertex_count) {
}

static void debug_draw_lines(const DDrawVertex* vertices, int vertex_count) {
  GL_CHECK(glUseProgram(s_debug_draw_lines_vb));
  GL_CHECK(glEnableVertexAttribArray(0));
  GL_CHECK(glEnableVertexAttribArray(1));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, s_debug_draw_lines_vb));
  GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_count * sizeof(DDrawVertex), vertices));
  GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DDrawVertex), (void*)offsetof(DDrawVertex, pos_x)));
  GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DDrawVertex), (void*)offsetof(DDrawVertex, col_r)));

  // add a transform to rotation Z up to Y up
  // NOTE: this is applied to the view transform (inverse of the camera world transform)
  vectorial::mat4f makeYUp = vectorial::mat4f::axisRotation(-1.5708f, vectorial::vec3f(1.0f, 0.0f, 0.0f));
  const vectorial::mat4f world_view =
      makeYUp * vectorial::inverse(makeCameraTransform(&s_camera)) * s_models.back().transform;
  const vectorial::mat4f world_view_proj = s_camera.projection * world_view;
  bind_constant_mat4(s_debug_draw_lines_vb, "world_view_proj", world_view_proj);

  GL_CHECK(glDrawArrays(GL_LINES, 0, vertex_count));

  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL_CHECK(glDisableVertexAttribArray(1));
  GL_CHECK(glDisableVertexAttribArray(0));
  GL_CHECK(glUseProgram(0));
}

static void debug_draw_init() {
  DDrawSettings settings;
  ddraw_settings_init(&settings);
  settings.draw_points = &debug_draw_points;
  settings.draw_lines = &debug_draw_lines;

  // create the VBs
  GL_CHECK(glGenBuffers(1, &s_debug_draw_points_vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, s_debug_draw_points_vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, settings.max_points * sizeof(DDrawVertex), nullptr, GL_DYNAMIC_DRAW));
  GL_CHECK(glGenBuffers(1, &s_debug_draw_lines_vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, s_debug_draw_lines_vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, settings.max_lines * sizeof(DDrawVertex), nullptr, GL_DYNAMIC_DRAW));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

  s_debug_draw_program = load_shader("gi-demo/data/shaders/debug_draw");

  ddraw_init(&settings);
}

static void debug_draw_shutdown() {
  ddraw_shutdown();

  // destroy the VBs
  GL_CHECK(glDeleteBuffers(1, &s_debug_draw_lines_vb));
  GL_CHECK(glDeleteBuffers(1, &s_debug_draw_points_vb));
  s_debug_draw_lines_vb = 0;
  s_debug_draw_points_vb = 0;
}

static void init(bool reset) {
  GL_CHECK(glGenVertexArrays(1, &s_default_vao));
  GL_CHECK(glBindVertexArray(s_default_vao));

  debug_draw_init();

  load_models();
  load_shaders();

  if (!reset) {
    s_camera.pos = vectorial::vec3f(0.0f, -20.0f, 10.0f);
    s_camera.pitch = 0.0f;
    s_camera.yaw = 0.0f;
    s_camera.near = 0.01f;
    s_camera.far = 100.0f;
    s_camera.projection = vectorial::mat4f::perspective(1.3f, 1.7f, s_camera.near, s_camera.far);

    s_light.pos = vectorial::vec3f(0.0f, -8.0f, 10.0f);
    s_light.color = vectorial::vec3f(1.0f, 1.0f, 1.0f);
    s_light.intensity = 1.0f;
    s_light.range = 15.0f;
  }
}

static void destroy() {
  unload_shaders();
  unload_models();

  debug_draw_shutdown();

  s_models.clear();
  GL_CHECK(glBindVertexArray(0));
  GL_CHECK(glDeleteVertexArrays(1, &s_default_vao));
}

extern "C" void app_input_key_down(AppKeyCode key) {
  s_keyStatus[key] = true;
}

extern "C" void app_input_key_up(AppKeyCode key) {
  s_keyStatus[key] = false;
}

extern "C" void app_render(float dt) {
  if (s_first_draw) {
    s_first_draw = false;
    debug_draw_init();
    init(false);
  }
  s_time += dt;
  // float color_val = sinf(s_time);
  float color_val = 0.4f;

  // compute the camera's orientation
  vectorial::mat4f cameraOld = makeCameraTransform(&s_camera);
  vectorial::vec3f fwd = cameraOld.value.y;
  vectorial::vec3f right = cameraOld.value.x;
  vectorial::vec3f up = cameraOld.value.z;

  // apply inputs
  float rotateAngle = 3.14159 * dt;
  float moveDistance = 50.0f * dt;
  if (s_keyStatus[APP_KEY_CODE_LSHIFT] || s_keyStatus[APP_KEY_CODE_RSHIFT]) {
    rotateAngle *= 2.0f;
    moveDistance *= 5.0f;
  }

  if (s_keyStatus[APP_KEY_CODE_R]) {
    destroy();
    init(true);
  }
  if (s_keyStatus[APP_KEY_CODE_F1]) {
    s_draw_wireframe = !s_draw_wireframe;
  }
  if (s_keyStatus[APP_KEY_CODE_F2]) {
    s_draw_depth = !s_draw_depth;
  }

  if (s_keyStatus[APP_KEY_CODE_LCONTROL]) {
    if (s_keyStatus[APP_KEY_CODE_A]) {
      s_light.pos -= vectorial::vec3f(1.0f, 0.0f, 0.0f) * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_D]) {
      s_light.pos += vectorial::vec3f(1.0f, 0.0f, 0.0f) * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_S]) {
      s_light.pos -= vectorial::vec3f(0.0f, 1.0f, 0.0f) * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_W]) {
      s_light.pos += vectorial::vec3f(0.0f, 1.0f, 0.0f) * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_Q]) {
      s_light.pos -= vectorial::vec3f(0.0f, 0.0f, 1.0f) * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_E]) {
      s_light.pos += vectorial::vec3f(0.0f, 0.0f, 1.0f) * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_UP]) {
      s_light.intensity += 5.0f * dt;
    }
    if (s_keyStatus[APP_KEY_CODE_DOWN]) {
      s_light.intensity -= 5.0f * dt;
    }
    if (s_keyStatus[APP_KEY_CODE_LEFT]) {
      s_light.range -= 5.0f * dt;
    }
    if (s_keyStatus[APP_KEY_CODE_RIGHT]) {
      s_light.range += 5.0f * dt;
    }
  }
  else {
    if (s_keyStatus[APP_KEY_CODE_A]) {
      s_camera.pos -= right * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_D]) {
      s_camera.pos += right * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_E]) {
      s_camera.pos += up * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_Q]) {
      s_camera.pos -= up * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_W]) {
      s_camera.pos += fwd * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_S]) {
      s_camera.pos -= fwd * moveDistance;
    }
    if (s_keyStatus[APP_KEY_CODE_LEFT]) {
      s_camera.yaw += rotateAngle;
    }
    if (s_keyStatus[APP_KEY_CODE_RIGHT]) {
      s_camera.yaw -= rotateAngle;
    }
    if (s_keyStatus[APP_KEY_CODE_UP]) {
      s_camera.pitch += rotateAngle;
    }
    if (s_keyStatus[APP_KEY_CODE_DOWN]) {
      s_camera.pitch -= rotateAngle;
    }
  }

  // build the camera's world transform
  vectorial::mat4f view = vectorial::inverse(makeCameraTransform(&s_camera));

  // render
  GL_CHECK(glClearColor(color_val, color_val, color_val, 0.0f));
  GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  GL_CHECK(glEnable(GL_DEPTH_TEST));
  GL_CHECK(glDepthFunc(GL_LESS));
  GL_CHECK(glEnable(GL_CULL_FACE));
  GL_CHECK(glCullFace(GL_BACK));

  // draw all the models
  for (const auto& model : s_models) {
    if (model.wireframe || s_draw_wireframe) {
      GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
    }
    else {
      GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
    }

    GLuint program;
    if (s_draw_depth) {
      program = s_program_depth;
    }
    else {
      program = s_program;
    }
    GL_CHECK(glUseProgram(program));
    bind_constants(program, model.transform, view, s_camera.projection);

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model.vb));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr));
    GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, n)));
    GL_CHECK(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, c)));
    GL_CHECK(glDrawElements(GL_TRIANGLES, model.tri_count * 3, GL_UNSIGNED_SHORT, nullptr));
    GL_CHECK(glDisableVertexAttribArray(0));
    GL_CHECK(glDisableVertexAttribArray(1));
    GL_CHECK(glDisableVertexAttribArray(2));
  }

  for (const auto& normal : s_debug_normals) {
    float pos[3];
    pos[0] = normal.p.x;
    pos[1] = normal.p.y;
    pos[2] = normal.p.z;
    float nor[3];
    nor[0] = normal.n.x;
    nor[1] = normal.n.y;
    nor[2] = normal.n.z;
    float col[3] = {1.0f, 1.0f, 1.0f};
    ddraw_normal(pos, nor, col, 0.5f);
  }
  ddraw_flush();
}
