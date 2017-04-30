#include <OpenGL/gl3.h>
#include <math.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "app.h"
#include "vendor/tinyobjloader/tiny_obj_loader.h"

struct Model {
  GLuint ib;
  GLuint vb;
  int tri_count;
};

static bool s_first_draw = true;
static float s_time = 0.0f;
static std::vector<Model> s_models;

static GLuint s_default_vao;
static GLuint s_program;

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Vertex {
  Vec3 p;
  Vec3 n;
};

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

static void load_models() {
  char * dir = getcwd(NULL, 0);
  std::cout << "Current dir: " << dir << std::endl;

  const char* filename = "gi-demo.app/Contents/Resources/cornell_box.obj";
  const char* mtl_dirname = "gi-demo.app/Contents/Resources/";
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
  glGenBuffers(1, &model.ib);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), &indices[0], GL_STATIC_DRAW);

  glGenBuffers(1, &model.vb);
  glBindBuffer(GL_ARRAY_BUFFER, model.vb);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // finish up the model and save it
  model.tri_count = indices.size() / 3;
  s_models.push_back(model);
}

static void load_shaders() {
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

//  std::string vertex_code;
//  std::string fragment_code;
//  load_file(&vertex_code, "gi-demo.app/Contents/Resources/constant_color_vertex.glsl");
//  load_file(&fragment_code, "gi-demo.app/Contents/Resources/constant_color_fragment.glsl");

  const char* vertex_code =
    "#version 330 core\n"
    "layout(location = 0) in vec3 position;\n"
    "void main() {\n"
    "  gl_Position.xyz = position;\n"
    "  gl_Position.w = 1.0;\n"
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
  glShaderSource(vertex_shader, 1, &vertex_code, nullptr);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
  glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &info_log_length);
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    glGetShaderInfoLog(vertex_shader, info_log_length, nullptr, &err_msg[0]);
    std::cerr << "VERTEX SHADER ERROR: " << err_msg << std::endl;
  }

  glShaderSource(fragment_shader, 1, &fragment_code, nullptr);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
  glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &info_log_length);
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    glGetShaderInfoLog(fragment_shader, info_log_length, nullptr, &err_msg[0]);
    std::cerr << "FRAGMENT SHADER ERROR: " << err_msg << std::endl;
  }

  // link
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glGetShaderiv(program, GL_LINK_STATUS, &result);
  glGetShaderiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
  if (info_log_length > 0) {
    std::string err_msg(info_log_length, '\0');
    glGetProgramInfoLog(program, info_log_length, nullptr, &err_msg[0]);
    std::cerr << "SHADER LINK ERROR: " << err_msg << std::endl;
  }

  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  s_program = program;
}

static void init() {
  glGenVertexArrays(1, &s_default_vao);
  glBindVertexArray(s_default_vao);

  load_models();
  load_shaders();
}

extern "C" void app_render(float dt) {
  if (s_first_draw) {
    s_first_draw = false;
    init();
  }
  s_time += dt;
  float color_val = sinf(s_time);

  glClearColor(color_val, color_val, color_val, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // draw all the models
  for (const auto& model : s_models) {
    glUseProgram(s_program);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib);
    glBindBuffer(GL_ARRAY_BUFFER, model.vb);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glDrawElements(GL_TRIANGLES, model.tri_count, GL_UNSIGNED_SHORT, nullptr);
    glDisableVertexAttribArray(0);
  }
}
