#include <OpenGL/gl3.h>
#include <math.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vendor/tinyobjloader/tiny_obj_loader.h"
#include <vectorial/vectorial.h>
#include "app.h"

#define GL_CHECK_ENABLED 1

#if GL_CHECK_ENABLED
    #define GL_CHECK(expr) do { (expr); GLenum err = glGetError(); if (err != GL_NO_ERROR) { report_error("GL expr failed. expr=`%s` code=%04xh msg=%s\n", #expr, err, get_gl_error_description(err)); } } while (false)
#else
    #define GL_CHECK(expr) (expr)
#endif

struct Model {
  vectorial::mat4f transform;
  GLuint ib;
  GLuint vb;
  int tri_count;
};

struct Camera {
  vectorial::vec3f pos;
  float pitch;
  float yaw;
  vectorial::mat4f projection;
};

static bool s_first_draw = true;
static float s_time = 0.0f;
static std::vector<Model> s_models;
static Camera s_camera;

static GLuint s_default_vao;
static GLuint s_program;

static bool s_keyStatus[APP_KEY_CODE_COUNT];

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Vertex {
  Vec3 p;
  Vec3 n;
};

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

static void vec_sub(Vec3* out, const Vec3& a, const Vec3& b) {
  out->x = a.x - b.x;
  out->y = a.y - b.y;
  out->z = a.z - b.z;
}

static void vec_mul(Vec3* out, const Vec3& a, float scalar) {
  out->x = a.x * scalar;
  out->y = a.y * scalar;
  out->z = a.z * scalar;
}

static void vec_cross(Vec3* out, const Vec3& a, const Vec3& b) {
  out->x = a.y * b.z - a.z * b.y;
  out->y = a.z * b.x - a.x - b.z;
  out->z = a.x * b.y - a.y - b.x;
}

static void vec_norm(Vec3* out, const Vec3& a) {
  float len = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
  vec_mul(out, a, 1.0f / len);
}

//static void load_file(std::string* out, const char* filename) {
//  if (std::ifstream is{filename, std::ios::binary | std::ios::ate}) {
//    auto size = is.tellg();
//    out->resize(size, '\0');
//    is.seekg(0);
//    is.read(&str[0], size);
//  }
//}

static void normal_from_face(Vec3* out, const Vec3& p0, const Vec3& p1, const Vec3& p2) {
  Vec3 p01;
  Vec3 p02;
  Vec3 cross;
  vec_sub(&p01, p1, p0);
  vec_sub(&p02, p2, p0);
  vec_cross(&cross, p01, p02);
  vec_norm(out, cross);
}

static void load_model(const char* filename, const char* mtl_dirname) {
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

  std::vector<uint16_t> indices;
  std::vector<Vertex> vertices;

  for (const tinyobj::shape_t& shape : shapes) {
    for (size_t face = 0, face_count = shape.mesh.indices.size() / 3; face < face_count; ++face) {
      tinyobj::index_t idx0 = shape.mesh.indices[3 * face + 0];
      tinyobj::index_t idx1 = shape.mesh.indices[3 * face + 1];
      tinyobj::index_t idx2 = shape.mesh.indices[3 * face + 2];

      // positions
      Vec3 pos0, pos1, pos2;
      pos0.x = attrib.vertices[3 * idx0.vertex_index + 0];
      pos0.y = attrib.vertices[3 * idx0.vertex_index + 1];
      pos0.z = attrib.vertices[3 * idx0.vertex_index + 2];
      pos1.x = attrib.vertices[3 * idx1.vertex_index + 0];
      pos1.y = attrib.vertices[3 * idx1.vertex_index + 1];
      pos1.z = attrib.vertices[3 * idx1.vertex_index + 2];
      pos2.x = attrib.vertices[3 * idx2.vertex_index + 0];
      pos2.y = attrib.vertices[3 * idx2.vertex_index + 1];
      pos2.z = attrib.vertices[3 * idx2.vertex_index + 2];

      // normals
      Vec3 nor0, nor1, nor2;
      if (attrib.normals.size() > 0) {
        nor0.x = attrib.normals[3 * idx0.normal_index + 0];
        nor0.y = attrib.normals[3 * idx0.normal_index + 1];
        nor0.z = attrib.normals[3 * idx0.normal_index + 2];
        nor1.x = attrib.normals[3 * idx1.normal_index + 0];
        nor1.y = attrib.normals[3 * idx1.normal_index + 1];
        nor1.z = attrib.normals[3 * idx1.normal_index + 2];
        nor2.x = attrib.normals[3 * idx2.normal_index + 0];
        nor2.y = attrib.normals[3 * idx2.normal_index + 1];
        nor2.z = attrib.normals[3 * idx2.normal_index + 2];
      }
      else {
        Vec3 normal;
        normal_from_face(&normal, pos0, pos1, pos2);
        nor0 = normal;
        nor1 = normal;
        nor2 = normal;
      }

      Vertex v0 = { pos0, nor0 };
      Vertex v1 = { pos1, nor1 };
      Vertex v2 = { pos2, nor2 };
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
  int ib_size_bytes = indices.size() * sizeof(uint16_t);
  GL_CHECK(glGenBuffers(1, &model.ib));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib));
  GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, ib_size_bytes, ib_data, GL_STATIC_DRAW));

  const Vertex* vb_data = &vertices[0];
  int vb_size_bytes = vertices.size() * sizeof(Vertex);
  GL_CHECK(glGenBuffers(1, &model.vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model.vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vb_size_bytes, vb_data, GL_STATIC_DRAW));

  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

  // finish up the model and save it
  model.tri_count = indices.size() / 3;
  model.transform = vectorial::mat4f::identity();
  s_models.push_back(model);
}

static void load_models() {
//  char * dir = getcwd(NULL, 0);
//  std::cout << "Current dir: " << dir << std::endl;

  const char* mtl_dirname = "gi-demo.app/Contents/Resources/";
  load_model("gi-demo.app/Contents/Resources/floor.obj", mtl_dirname);
  load_model("gi-demo.app/Contents/Resources/cornell_box.obj", mtl_dirname);
}

static void load_shaders() {
  GLuint vertex_shader;
  GLuint fragment_shader;
  GL_CHECK(vertex_shader = glCreateShader(GL_VERTEX_SHADER));
  GL_CHECK(fragment_shader = glCreateShader(GL_FRAGMENT_SHADER));

//  std::string vertex_code;
//  std::string fragment_code;
//  load_file(&vertex_code, "gi-demo.app/Contents/Resources/constant_color_vertex.glsl");
//  load_file(&fragment_code, "gi-demo.app/Contents/Resources/constant_color_fragment.glsl");

  const char* vertex_code =
    "#version 330 core\n"
    "layout(location = 0) in vec3 position;\n"
    "uniform mat4 world_view_proj;\n"
    "void main() {\n"
    "  gl_Position = world_view_proj * vec4(position, 1.0);\n"
    "}\n"
  ;
  const char* fragment_code =
    "#version 330 core\n"
    "out vec3 color;\n"
    "void main() {\n"
    "  color = vec3(1, 0, 0);\n"
    "}\n"
  ;

  GLint result = GL_FALSE;
  int info_log_length;

  // compile
  GL_CHECK(glShaderSource(vertex_shader, 1, &vertex_code, nullptr));
  GL_CHECK(glCompileShader(vertex_shader));
  GL_CHECK(glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result));
  GL_CHECK(glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &info_log_length));
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    GL_CHECK(glGetShaderInfoLog(vertex_shader, info_log_length, nullptr, &err_msg[0]));
    std::cerr << "VERTEX SHADER ERROR: " << err_msg << std::endl;
  }

  GL_CHECK(glShaderSource(fragment_shader, 1, &fragment_code, nullptr));
  GL_CHECK(glCompileShader(fragment_shader));
  GL_CHECK(glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result));
  GL_CHECK(glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &info_log_length));
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    GL_CHECK(glGetShaderInfoLog(fragment_shader, info_log_length, nullptr, &err_msg[0]));
    std::cerr << "FRAGMENT SHADER ERROR: " << err_msg << std::endl;
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
    std::cerr << "SHADER LINK ERROR: " << err_msg << std::endl;
  }

  GL_CHECK(glDetachShader(program, vertex_shader));
  GL_CHECK(glDetachShader(program, fragment_shader));
  GL_CHECK(glDeleteShader(vertex_shader));
  GL_CHECK(glDeleteShader(fragment_shader));

  s_program = program;
}

static void init() {
  GL_CHECK(glGenVertexArrays(1, &s_default_vao));
  GL_CHECK(glBindVertexArray(s_default_vao));

  load_models();
  load_shaders();

  s_camera.pos = vectorial::vec3f(0.0f, -20.0f, 10.0f);
  s_camera.pitch = 0.0f;
  s_camera.yaw = 0.0f;
  s_camera.projection = vectorial::mat4f::perspective(1.3f, 1.7f, 0.01f, 1000.0f);
}

static void bind_constants(GLuint program, const vectorial::mat4f& world, const vectorial::mat4f& view, const vectorial::mat4f& proj) {
  // TODO: only do this when loading the shader
  GLuint world_view_proj_uniform;
  GL_CHECK(world_view_proj_uniform = glGetUniformLocation(program, "world_view_proj"));

  // add a transform to rotation Z up to Y up
  // NOTE: this is applied to the view transform (inverse of the camera world transform)
  vectorial::mat4f makeYUp = vectorial::mat4f::axisRotation(-1.5708f, vectorial::vec3f(1.0f, 0.0f, 0.0f));

  // set the constant
  vectorial::mat4f world_view_proj = proj * makeYUp * view * world;
  float world_view_proj_float[16];
  world_view_proj.store(world_view_proj_float);
  GL_CHECK(glUniformMatrix4fv(world_view_proj_uniform, 1, GL_FALSE, world_view_proj_float));
}

static vectorial::mat4f makeCameraTransform(Camera* cam) {
  vectorial::mat4f cameraYaw = vectorial::mat4f::axisRotation(cam->yaw, vectorial::vec3f(0.0f, 0.0f, 1.0f));
  vectorial::mat4f cameraPitch = vectorial::mat4f::axisRotation(cam->pitch, cameraYaw.value.x);
  return vectorial::mat4f::translation(cam->pos) * cameraPitch * cameraYaw;
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
    init();
  }
  s_time += dt;
  float color_val = sinf(s_time);

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
    s_camera.yaw += -rotateAngle;
  }
  if (s_keyStatus[APP_KEY_CODE_UP]) {
    s_camera.pitch += -rotateAngle;
  }
  if (s_keyStatus[APP_KEY_CODE_DOWN]) {
    s_camera.pitch += rotateAngle;
  }

  // build the camera's world transform
  vectorial::mat4f view = vectorial::inverse(makeCameraTransform(&s_camera));

  // render
  glClearColor(color_val, color_val, color_val, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // draw all the models
  bool first = true;
  for (const auto& model : s_models) {
    if (first) {
      GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
      first = false;
    }
    else {
      GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
    }

    bind_constants(s_program, model.transform, view, s_camera.projection);

    GL_CHECK(glUseProgram(s_program));
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model.vb));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr));
    GL_CHECK(glDrawElements(GL_TRIANGLES, model.tri_count * 3, GL_UNSIGNED_SHORT, nullptr));
    GL_CHECK(glDisableVertexAttribArray(0));
  }
}
