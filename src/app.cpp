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

#define MAX_CHANNELS 16

enum KeyStatus {
  KEY_STATUS_DOWN = 0x01,
  KEY_STATUS_EDGE = 0x02,
};

enum ChannelSemantic {
  CHANNEL_SEMANTIC_COLOR,
  CHANNEL_SEMANTIC_NORMAL,
  CHANNEL_SEMANTIC_POSITION,
  CHANNEL_SEMANTIC_TEXCOORD,
};

enum ChannelType {
  CHANNEL_TYPE_FLOAT_3,
  CHANNEL_TYPE_UBYTE_4,
};

struct VertexChannelDesc {
  ChannelType type;
  ChannelSemantic semantic;
};

struct Mesh {
  void* indices;
  void* vertices;
  VertexChannelDesc channels[MAX_CHANNELS];
  unsigned index_count;
  unsigned vertex_count;
  unsigned channel_count;
  bool index_size_32_bit;
};

struct Model {
  vectorial::mat4f transform;
  GLuint ib;
  GLuint vb;
  GLuint lightmap_vb;
  int tri_count;
  VertexChannelDesc channels[MAX_CHANNELS];
  unsigned channel_count;
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

struct LightmapTriangle {
  vectorial::vec2f positions[3];
  vectorial::vec2f uvs[3];
  float width;
  float height;
  int mesh_tri_index;
  int projected_edge_index;
};

static uint32_t s_brewer_colors[] = {
    0xa6cee3ff,
    0x1f78b4ff,
    0xb2df8aff,
    0x33a02cff,
    0xfb9a99ff,
    0xe31a1cff,
    0xfdbf6fff,
    0xff7f00ff,
    0xcab2d6ff,
    0x6a3d9aff,
    0xffff99ff,
    0xb15928ff,
};
#define BREWER_COLOR_COUNT 12

static float s_window_width;
static float s_window_height;

static bool s_first_draw = true;
static float s_time = 0.0f;
static std::vector<Model> s_models;
static Camera s_camera;
static Light s_light;

// debug
static bool s_draw_wireframe = false;
static bool s_draw_depth = false;
static bool s_draw_lightmap = false;
static bool s_vis_lightmap = false;
static int s_num_lightmap_tris = -1;

static GLuint s_default_vao;
static GLuint s_program;
static GLuint s_program_depth;
static GLuint s_program_lightmap_only;

static GLuint s_lightmap_pack_program;
static GLuint s_draw_texture_program;

static GLuint s_lightmap_tex_id;

static int s_key_status[APP_KEY_CODE_COUNT];

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

static int channel_size(const VertexChannelDesc* channel) {
  switch (channel->type) {
    case CHANNEL_TYPE_FLOAT_3:
      return 12;
    case CHANNEL_TYPE_UBYTE_4:
      return 4;
    default:
      assert(false && "unknown channel type");
      return 0;
  }
}

static int channel_elements(const VertexChannelDesc* channel) {
  switch (channel->type) {
    case CHANNEL_TYPE_FLOAT_3:
      return 3;
    case CHANNEL_TYPE_UBYTE_4:
      return 4;
    default:
      assert(false && "unknown channel type");
      return 0;
  }
}

static GLenum to_gl_channel_type(ChannelType type) {
  switch (type) {
    case CHANNEL_TYPE_FLOAT_3:
      return GL_FLOAT;
    case CHANNEL_TYPE_UBYTE_4:
      return GL_UNSIGNED_BYTE;
    default:
      assert(false && "unknown channel type");
      return 0;
  }
}

static int vertex_stride(const VertexChannelDesc* channels, int channel_count) {
  int stride = 0;
  for (int index = 0; index < channel_count; ++index) {
    stride += channel_size(channels + index);
  }
  return stride;
}

static vectorial::vec4f color_rgba_to_float4(uint32_t in) {
  float r = (float)((in >> 24) & 0xff) / 255.0f;
  float g = (float)((in >> 16) & 0xff) / 255.0f;
  float b = (float)((in >> 8) & 0xff) / 255.0f;
  float a = (float)((in >> 0) & 0xff) / 255.0f;
  return vectorial::vec4f(r, g, b, a);
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

static void bind_constant_vec4(GLuint program, const char* name, const vectorial::vec4f& value) {
  float value_f[4];
  value.store(value_f);

  // TODO: cache the uniform id
  GLuint uniform_id;
  GL_CHECK(uniform_id = glGetUniformLocation(program, name));
  GL_CHECK(glUniform4fv(uniform_id, 1, value_f));
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

static bool lightmap_project_triangles(std::vector<LightmapTriangle>& triangles, const Mesh* mesh) {
  // find the channel with the positions
  unsigned position_channel = 0xffffffffU;
  unsigned offset = 0;
  for (unsigned index = 0; index < mesh->channel_count; ++index) {
    if (mesh->channels[index].semantic == CHANNEL_SEMANTIC_POSITION) {
      position_channel = index;
      break;
    }
    offset += channel_size(mesh->channels + index);
  }
  if (position_channel == 0xffffffffU) {
    return false;
  }

  if (mesh->channels[position_channel].type != CHANNEL_TYPE_FLOAT_3) {
    return false;
  }

  triangles.reserve(mesh->index_count / 3);

  const unsigned stride = vertex_stride(mesh->channels, mesh->channel_count);
  for (unsigned tri_index0 = 0; tri_index0 < mesh->index_count; tri_index0 += 3) {
    const uint16_t* indices = (const uint16_t*)mesh->indices + tri_index0;
    const uint16_t index0 = indices[0];
    const uint16_t index1 = indices[1];
    const uint16_t index2 = indices[2];
    const float* pos_data0 = (const float*)((char*)mesh->vertices + offset + (stride * index0));
    const float* pos_data1 = (const float*)((char*)mesh->vertices + offset + (stride * index1));
    const float* pos_data2 = (const float*)((char*)mesh->vertices + offset + (stride * index2));

    vectorial::vec3f positions[3];
    positions[0].load(pos_data0);
    positions[1].load(pos_data1);
    positions[2].load(pos_data2);

    // find the longest edge
    vectorial::vec3f edges[3];
    edges[0] = positions[1] - positions[0];
    edges[1] = positions[2] - positions[1];
    edges[2] = positions[0] - positions[2];
    float lengths[3];
    lengths[0] = vectorial::length(edges[0]);
    lengths[1] = vectorial::length(edges[1]);
    lengths[2] = vectorial::length(edges[2]);
    int longest_edge_index;
    if (lengths[0] > lengths[1] && lengths[0] > lengths[2]) {
      longest_edge_index = 0;
    }
    else if (lengths[1] > lengths[0] && lengths[1] > lengths[2]) {
      longest_edge_index = 1;
    }
    else {
      longest_edge_index = 2;
    }

    int sorted_indices[3];
    sorted_indices[0] = longest_edge_index;
    sorted_indices[1] = (longest_edge_index + 1) % 3;
    sorted_indices[2] = (longest_edge_index + 2) % 3;

    // assuming the longest edge is on the x axis, find the height of the triangle using Heron's formula
    //  - a = {longest edge}
    //  - s = (a+b+c)/2
    //  - A = sqrt(s(s-a)(s-b)(s-c))
    //  - A = 0.5ah
    // => h = A/(0.5a)
    // const float a = lengths[sorted_indices[0]];
    // const float b = lengths[sorted_indices[1]];
    // const float c = lengths[sorted_indices[2]];
    // const float s = (a + b + c) * 0.5f;
    // const float area = sqrtf(s * (s - a) * (s - b) * (s - c));
    // const float h = area / (0.5 * a);

    // project the triangle to an XY plane
    vectorial::vec2f projected[3];
    projected[0] = vectorial::vec2f::zero();
    projected[1] = vectorial::vec2f(lengths[longest_edge_index], 0.0f);

    // using the dot product, derive the projected length of the third edge
    // dp = |a||b|cos(theta)
    const vectorial::vec3f edge_a = vectorial::normalize(positions[sorted_indices[1]] - positions[sorted_indices[0]]);
    const vectorial::vec3f edge_c = vectorial::normalize(positions[sorted_indices[2]] - positions[sorted_indices[0]]);
    const float cos_ac = vectorial::dot(edge_a, edge_c);
    const float sin_ac = sqrtf(1.0f - (cos_ac * cos_ac));
    projected[2] = vectorial::vec2f(lengths[sorted_indices[2]] * cos_ac, lengths[sorted_indices[2]] * sin_ac);

    // assuming the longest edge is on the x axis, find the height of the triangle using Heron's formula
    //  - a = {longest edge}
    //  - s = (a+b+c)/2
    //  - A = sqrt(s(s-a)(s-b)(s-c))
    //  - A = 0.5ah
    // => h = A/(0.5a)
    const float a = lengths[sorted_indices[0]];
    const float b = lengths[sorted_indices[1]];
    const float c = lengths[sorted_indices[2]];
    const float s = (a + b + c) * 0.5f;
    const float area = sqrtf(s * (s - a) * (s - b) * (s - c));
    const float h = area / (0.5 * a);

    LightmapTriangle tri;
    tri.positions[0] = projected[0];
    tri.positions[1] = projected[1];
    tri.positions[2] = projected[2];
    tri.width = lengths[sorted_indices[0]];
    tri.height = h;
    tri.mesh_tri_index = tri_index0 / 3;
    tri.projected_edge_index = longest_edge_index;
    triangles.push_back(tri);
  }

  return true;
}

static void lightmap_pack_texture(std::vector<LightmapTriangle>& triangles, int tex_width, int tex_height) {
  // const size_t texel_count = tex_width * tex_height;
  // std::vector<bool> used(texel_count, false);

  const vectorial::vec2f tex_scale(1.0f / tex_width, 1.0f / tex_height);
  const vectorial::vec2f vtx_scale = tex_scale * 2.0f;
  const vectorial::vec2f vtx_offset(-1.0f, -1.0f);

  // reverse sort the triangles by height
  std::sort(triangles.begin(), triangles.end(), [](const LightmapTriangle& a, const LightmapTriangle& b) {
    return a.height > b.height;
  });

  GLuint framebuf_id;
  GL_CHECK(glGenFramebuffers(1, &framebuf_id));
  GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, framebuf_id));

  GL_CHECK(glGenTextures(1, &s_lightmap_tex_id));
  GLuint tex_id = s_lightmap_tex_id;
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex_id));
  GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

  GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0));
  GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0};
  GL_CHECK(glDrawBuffers(1, draw_buffers));

  GLenum framebuf_status;
  GL_CHECK(framebuf_status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
  if (framebuf_status != GL_FRAMEBUFFER_COMPLETE) {
    printf("framebuf status not complete: %d\n", framebuf_status);
    exit(1);
  }

  GL_CHECK(glViewport(0, 0, tex_width, tex_height));
  GL_CHECK(glUseProgram(s_lightmap_pack_program));

  GLuint vb;
  GL_CHECK(glGenBuffers(1, &vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb));
  GL_CHECK(glEnableVertexAttribArray(0));
  GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr));

  GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
  GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

  const int padding = 2;
  bool flip = false;
  float dp_prev = 1.0f;
  int row_height = -1.0f;
  int u_top = -2;
  int u_bottom = -2;
  int v = 0;
  int color_index = 0;
  int tri_index = 0;
  for (LightmapTriangle& tri : triangles) {
    if (s_num_lightmap_tris >= 0 && tri_index >= s_num_lightmap_tris) {
      break;
    }
    ++tri_index;

    int tri_width = (int)(tri.width + 0.5f);
    int tri_height = (int)(tri.height + 0.5f);
    if (row_height < 0) {
      row_height = tri_height;
    }

    const uint32_t color_uint32 = s_brewer_colors[color_index];
    color_index = (color_index + 1) % BREWER_COLOR_COUNT;
    vectorial::vec4f color = color_rgba_to_float4(color_uint32);
    bind_constant_vec4(s_lightmap_pack_program, "u_color", color);

    // place the triangle at a point where either the base starts 2px from the previous top pt or the top starts 2px
    // from the previous base pt (whichever is farther)
    vectorial::vec2f vec_10 = vectorial::normalize(tri.positions[0] - tri.positions[1]);
    vectorial::vec2f vec_12 = vectorial::normalize(tri.positions[2] - tri.positions[1]);
    const float dp = vectorial::dot(vec_12, vec_10);
    int u;
    if (dp < dp_prev) {
      // offset from the base
      u = u_bottom + padding;
    }
    else {
      // offset from the top
      u = u_top + padding;
    }

    // check if this will wrap us around the end of the buffer
    if (u + tri_width > tex_width) {
      v += row_height;
      row_height = tri_height;
    }

    vectorial::vec2f uv_offset(u, v);
    vectorial::vec2f uv_pos0 = (tri.positions[0] + uv_offset);
    vectorial::vec2f uv_pos1 = (tri.positions[1] + uv_offset);
    vectorial::vec2f uv_pos2 = (tri.positions[2] + uv_offset);
    if (flip) {
      uv_pos0 = vectorial::vec2f(uv_pos0.x(), tri_height - uv_pos0.y());
      uv_pos1 = vectorial::vec2f(uv_pos1.x(), tri_height - uv_pos1.y());
      uv_pos2 = vectorial::vec2f(uv_pos2.x(), tri_height - uv_pos2.y());
    }

    tri.uvs[0] = uv_pos0 * tex_scale;
    tri.uvs[1] = uv_pos1 * tex_scale;
    tri.uvs[2] = uv_pos2 * tex_scale;

    uv_pos0 = (uv_pos0 * vtx_scale) + vtx_offset;
    uv_pos1 = (uv_pos1 * vtx_scale) + vtx_offset;
    uv_pos2 = (uv_pos2 * vtx_scale) + vtx_offset;

    // fill the VB with the new triangle
    float positions[6];
    uv_pos2.store(positions);
    uv_pos1.store(positions + 2);
    uv_pos0.store(positions + 4);
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), positions, GL_STATIC_DRAW));

    // draw the triangle into the buffer
    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));

    u_bottom = (tri.positions[1] + uv_offset).x();
    u_top = (tri.positions[2] + uv_offset).x();
    dp_prev = dp;
    flip = !flip;
  }

  GL_CHECK(glDisableVertexAttribArray(0));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL_CHECK(glDeleteBuffers(1, &vb));
  GL_CHECK(glUseProgram(0));
  GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  GL_CHECK(glDeleteFramebuffers(1, &framebuf_id));

  // sort the triangles by mesh order
  std::sort(triangles.begin(), triangles.end(), [](const LightmapTriangle& a, const LightmapTriangle& b) {
    return a.mesh_tri_index < b.mesh_tri_index;
  });
}

static GLuint lightmap_create_vb(const std::vector<LightmapTriangle>& lightmap_triangles) {
  const size_t tri_count = lightmap_triangles.size();
  const size_t uv_count = tri_count * 3;
  const size_t vb_size_bytes = uv_count * 2 * sizeof(float);
  float* uv_data = (float*)malloc(vb_size_bytes);

  int uv_index = 0;
  for (const LightmapTriangle& tri : lightmap_triangles) {
    int out_index0 = tri.projected_edge_index;
    int out_index1 = (tri.projected_edge_index + 1) % 3;
    int out_index2 = (tri.projected_edge_index + 2) % 3;
    tri.uvs[out_index0].store((uv_data + (2 * uv_index + 0)));
    tri.uvs[out_index1].store((uv_data + (2 * uv_index + 2)));
    tri.uvs[out_index2].store((uv_data + (2 * uv_index + 4)));
    uv_index += 3;
  }

  GLuint vb;
  GL_CHECK(glGenBuffers(1, &vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, uv_count * 2 * sizeof(float), uv_data, GL_STATIC_DRAW));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

  free(uv_data);
  return vb;
}

static Mesh* mesh_load(const char* filename, const char* mtl_dirname, const vectorial::mat4f& transform) {
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

  Mesh* mesh = (Mesh*)malloc(sizeof(Mesh));
  mesh->channels[0] = {CHANNEL_TYPE_FLOAT_3, CHANNEL_SEMANTIC_POSITION};
  mesh->channels[1] = {CHANNEL_TYPE_FLOAT_3, CHANNEL_SEMANTIC_NORMAL};
  mesh->channels[2] = {CHANNEL_TYPE_FLOAT_3, CHANNEL_SEMANTIC_COLOR};
  mesh->index_count = (unsigned)indices.size();
  mesh->vertex_count = (unsigned)vertices.size();
  mesh->channel_count = 3;
  mesh->index_size_32_bit = false;

  const unsigned ib_size = mesh->index_count * sizeof(uint16_t);
  const unsigned vb_size = mesh->vertex_count * vertex_stride(mesh->channels, mesh->channel_count);
  mesh->indices = malloc(ib_size);
  mesh->vertices = malloc(vb_size);
  memmove(mesh->indices, &indices[0], ib_size);
  memmove(mesh->vertices, &vertices[0], vb_size);

  return mesh;
}

static void mesh_destroy(Mesh* mesh) {
  free(mesh->indices);
  free(mesh->vertices);
  free(mesh);
}

static void model_create(Model* model, const Mesh* mesh, GLuint lightmap_vb) {
  model->ib = 0;
  model->vb = 0;
  model->lightmap_vb = lightmap_vb;
  model->tri_count = 0;
  model->wireframe = false;
  model->channel_count = mesh->channel_count;
  memmove(model->channels, mesh->channels, mesh->channel_count * sizeof(VertexChannelDesc));

  // create the index buffer
  int ib_size_bytes;
  if (mesh->index_size_32_bit) {
    ib_size_bytes = mesh->index_count * sizeof(uint32_t);
  }
  else {
    ib_size_bytes = mesh->index_count * sizeof(uint16_t);
  }

  GL_CHECK(glGenBuffers(1, &model->ib));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ib));
  GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, ib_size_bytes, mesh->indices, GL_STATIC_DRAW));
  GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

  int vb_size_bytes = mesh->vertex_count * vertex_stride(mesh->channels, mesh->channel_count);
  GL_CHECK(glGenBuffers(1, &model->vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model->vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vb_size_bytes, mesh->vertices, GL_STATIC_DRAW));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

  // finish up the model and save it
  model->tri_count = mesh->index_count / 3;
  model->transform = vectorial::mat4f::identity();
}

static void model_create(const Mesh* mesh, GLuint lightmap_vb) {
  Model model;
  model_create(&model, mesh, lightmap_vb);
  s_models.push_back(model);
}

static void model_destroy(Model* model) {
  GL_CHECK(glDeleteBuffers(1, &model->ib));
  GL_CHECK(glDeleteBuffers(1, &model->vb));
}

static void draw_debug_texture(GLuint tex_id, float pos_x, float pos_y, float width, float height) {
  float vb_data[24];
  vb_data[0] = pos_x;
  vb_data[1] = pos_y;
  vb_data[2] = 0.0f;
  vb_data[3] = 0.0f;

  vb_data[4] = pos_x + width;
  vb_data[5] = pos_y;
  vb_data[6] = 1.0f;
  vb_data[7] = 0.0f;

  vb_data[8] = pos_x + width;
  vb_data[9] = pos_y + height;
  vb_data[10] = 1.0f;
  vb_data[11] = 1.0f;

  vb_data[12] = pos_x;
  vb_data[13] = pos_y;
  vb_data[14] = 0.0f;
  vb_data[15] = 0.0f;

  vb_data[16] = pos_x + width;
  vb_data[17] = pos_y + height;
  vb_data[18] = 1.0f;
  vb_data[19] = 1.0f;

  vb_data[20] = pos_x;
  vb_data[21] = pos_y + height;
  vb_data[22] = 0.0f;
  vb_data[23] = 1.0f;

  GLuint vb;
  GL_CHECK(glGenBuffers(1, &vb));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb));
  GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, vb_data, GL_STATIC_DRAW));

  GL_CHECK(glEnableVertexAttribArray(0));
  GL_CHECK(glEnableVertexAttribArray(1));
  GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr));
  GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))));

  GL_CHECK(glActiveTexture(GL_TEXTURE0));
  GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex_id));

  GL_CHECK(glDisable(GL_DEPTH_TEST));
  GL_CHECK(glUseProgram(s_draw_texture_program));

  GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 6));

  GL_CHECK(glUseProgram(0));
  GL_CHECK(glEnable(GL_DEPTH_TEST));
  GL_CHECK(glEnableVertexAttribArray(1));
  GL_CHECK(glEnableVertexAttribArray(0));
  GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL_CHECK(glDeleteBuffers(1, &vb));
}

static void load_models() {
  // char * dir = getcwd(NULL, 0);
  // std::cout << "Current dir: " << dir << std::endl;

  const char* mtl_dirname = "data/";
  Mesh* mesh = mesh_load("data/cornell_box.obj",
                         mtl_dirname,
                         vectorial::mat4f::scale(10.0f) *
                             vectorial::mat4f::axisRotation(1.5708f, vectorial::vec3f(1.0f, 0.0f, 0.0f)));

  std::vector<LightmapTriangle> lightmap_triangles;
  GLuint lightmap_vb = 0;
  if (lightmap_project_triangles(lightmap_triangles, mesh)) {
    lightmap_pack_texture(lightmap_triangles, 512, 512);
    lightmap_vb = lightmap_create_vb(lightmap_triangles);
  }

  model_create(mesh, lightmap_vb);
  mesh_destroy(mesh);
}

static void unload_models() {
  for (Model& model : s_models) {
    model_destroy(&model);
  }
  s_models.clear();
}

static void load_shaders() {
  s_program = load_shader("data/shaders/lit");
  s_program_lightmap_only = load_shader("data/shaders/lightmap_only");
  s_program_depth = load_shader("data/shaders/lit.vs.glsl", "data/shaders/depth.fs.glsl");
  s_lightmap_pack_program = load_shader("data/shaders/lightmap_pack");
  s_draw_texture_program = load_shader("data/shaders/debug_texture");
}

static void unload_shaders() {
  GL_CHECK(glDeleteProgram(s_lightmap_pack_program));
  GL_CHECK(glDeleteProgram(s_program_depth));
  GL_CHECK(glDeleteProgram(s_program));
  s_lightmap_pack_program = 0;
  s_program_depth = 0;
  s_program = 0;
}

static void camera_set_projection(Camera* cam, float fov_y, float width, float height) {
  float aspect = 1.7f;
  if (height > 0.0f) {
    aspect = width / height;
  }
  cam->projection = vectorial::mat4f::perspective(fov_y, aspect, cam->near, cam->far);
}

static vectorial::mat4f makeCameraTransform(Camera* cam) {
  vectorial::mat4f cameraYaw = vectorial::mat4f::axisRotation(cam->yaw, vectorial::vec3f(0.0f, 0.0f, 1.0f));
  vectorial::mat4f cameraPitch = vectorial::mat4f::axisRotation(cam->pitch, cameraYaw.value.x);
  return vectorial::mat4f::translation(cam->pos) * cameraPitch * cameraYaw;
}

static void debug_draw_points(const DDrawVertex* vertices, int vertex_count) {
}

static void debug_draw_lines(const DDrawVertex* vertices, int vertex_count) {
  GL_CHECK(glUseProgram(s_debug_draw_program));
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

  s_debug_draw_program = load_shader("data/shaders/debug_draw");

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

static void draw_models(const Model* models, unsigned model_count, const vectorial::mat4f& view) {
  for (unsigned index = 0; index < model_count; ++index) {
    const Model& model = models[index];

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
    else if (s_draw_lightmap) {
      program = s_program_lightmap_only;
    }
    else {
      program = s_program;
    }
    GL_CHECK(glUseProgram(program));
    bind_constants(program, model.transform, view, s_camera.projection);

    // bind the lightmap texture
    GL_CHECK(glActiveTexture(GL_TEXTURE0));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, s_lightmap_tex_id));

    const unsigned stride = vertex_stride(model.channels, model.channel_count);
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ib));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model.vb));
    size_t offset = 0;
    for (int index = 0; index < model.channel_count; ++index) {
      const VertexChannelDesc* channel = model.channels + index;
      GL_CHECK(glEnableVertexAttribArray(index));
      GL_CHECK(glVertexAttribPointer(
          index, channel_elements(channel), to_gl_channel_type(channel->type), GL_FALSE, stride, (void*)offset));
      offset += channel_size(channel);
    }
    GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, n)));
    GL_CHECK(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, c)));

    if (model.lightmap_vb) {
      GL_CHECK(glEnableVertexAttribArray(15));
      GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, model.lightmap_vb));
      GL_CHECK(glVertexAttribPointer(15, 2, GL_FLOAT, GL_FALSE, 0, nullptr));
    }

    GL_CHECK(glDrawElements(GL_TRIANGLES, model.tri_count * 3, GL_UNSIGNED_SHORT, nullptr));

    if (model.lightmap_vb) {
      GL_CHECK(glDisableVertexAttribArray(15));
    }
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    for (int index = 0; index < model.channel_count; ++index) {
      GL_CHECK(glDisableVertexAttribArray(index));
    }
  }
}

static void init(bool reset) {
  GL_CHECK(glGenVertexArrays(1, &s_default_vao));
  GL_CHECK(glBindVertexArray(s_default_vao));

  debug_draw_init();

  load_shaders();
  load_models();

  if (!reset) {
    s_camera.pos = vectorial::vec3f(0.0f, -20.0f, 10.0f);
    s_camera.pitch = 0.0f;
    s_camera.yaw = 0.0f;
    s_camera.near = 0.01f;
    s_camera.far = 100.0f;
    camera_set_projection(&s_camera, 1.3f, s_window_width, s_window_height);

    s_light.pos = vectorial::vec3f(0.0f, -8.0f, 10.0f);
    s_light.color = vectorial::vec3f(1.0f, 1.0f, 1.0f);
    s_light.intensity = 1.0f;
    s_light.range = 15.0f;
  }
}

static void destroy() {
  unload_models();
  unload_shaders();

  debug_draw_shutdown();

  s_models.clear();
  GL_CHECK(glBindVertexArray(0));
  GL_CHECK(glDeleteVertexArrays(1, &s_default_vao));
}

static bool is_key_down(AppKeyCode key) {
  return 0 != (s_key_status[key] & KEY_STATUS_DOWN);
}
static bool is_key_edge_down(AppKeyCode key) {
  return (KEY_STATUS_DOWN | KEY_STATUS_EDGE) == (s_key_status[key] & (KEY_STATUS_DOWN | KEY_STATUS_EDGE));
}
static void clear_key_edge_states() {
  for (int index = 0; index < APP_KEY_CODE_COUNT; ++index) {
    s_key_status[index] &= ~KEY_STATUS_EDGE;
  }
}

extern "C" void app_input_key_down(AppKeyCode key) {
  s_key_status[key] = KEY_STATUS_DOWN | KEY_STATUS_EDGE;
}

extern "C" void app_input_key_up(AppKeyCode key) {
  s_key_status[key] = KEY_STATUS_EDGE;
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
  if (is_key_down(APP_KEY_CODE_LSHIFT) || is_key_down(APP_KEY_CODE_RSHIFT)) {
    rotateAngle *= 2.0f;
    moveDistance *= 5.0f;
  }

  if (is_key_edge_down(APP_KEY_CODE_R)) {
    destroy();
    init(true);
  }
  if (is_key_edge_down(APP_KEY_CODE_F1)) {
    s_draw_wireframe = !s_draw_wireframe;
  }
  if (is_key_edge_down(APP_KEY_CODE_F2)) {
    s_draw_depth = !s_draw_depth;
  }
  if (is_key_edge_down(APP_KEY_CODE_F3)) {
    s_draw_lightmap = !s_draw_lightmap;
  }
  if (is_key_edge_down(APP_KEY_CODE_F5)) {
    s_vis_lightmap = !s_vis_lightmap;
  }
  if (is_key_edge_down(APP_KEY_CODE_MINUS)) {
    --s_num_lightmap_tris;
    if (s_num_lightmap_tris < -1) {
      s_num_lightmap_tris = -1;
    }
  }
  if (is_key_edge_down(APP_KEY_CODE_EQUAL)) {
    ++s_num_lightmap_tris;
  }

  if (is_key_down(APP_KEY_CODE_LCONTROL)) {
    if (is_key_down(APP_KEY_CODE_A)) {
      s_light.pos -= vectorial::vec3f(1.0f, 0.0f, 0.0f) * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_D)) {
      s_light.pos += vectorial::vec3f(1.0f, 0.0f, 0.0f) * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_S)) {
      s_light.pos -= vectorial::vec3f(0.0f, 1.0f, 0.0f) * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_W)) {
      s_light.pos += vectorial::vec3f(0.0f, 1.0f, 0.0f) * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_Q)) {
      s_light.pos -= vectorial::vec3f(0.0f, 0.0f, 1.0f) * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_E)) {
      s_light.pos += vectorial::vec3f(0.0f, 0.0f, 1.0f) * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_UP)) {
      s_light.intensity += 5.0f * dt;
    }
    if (is_key_down(APP_KEY_CODE_DOWN)) {
      s_light.intensity -= 5.0f * dt;
    }
    if (is_key_down(APP_KEY_CODE_LEFT)) {
      s_light.range -= 5.0f * dt;
    }
    if (is_key_down(APP_KEY_CODE_RIGHT)) {
      s_light.range += 5.0f * dt;
    }
  }
  else {
    if (is_key_down(APP_KEY_CODE_A)) {
      s_camera.pos -= right * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_D)) {
      s_camera.pos += right * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_E)) {
      s_camera.pos += up * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_Q)) {
      s_camera.pos -= up * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_W)) {
      s_camera.pos += fwd * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_S)) {
      s_camera.pos -= fwd * moveDistance;
    }
    if (is_key_down(APP_KEY_CODE_LEFT)) {
      s_camera.yaw += rotateAngle;
    }
    if (is_key_down(APP_KEY_CODE_RIGHT)) {
      s_camera.yaw -= rotateAngle;
    }
    if (is_key_down(APP_KEY_CODE_UP)) {
      s_camera.pitch += rotateAngle;
    }
    if (is_key_down(APP_KEY_CODE_DOWN)) {
      s_camera.pitch -= rotateAngle;
    }
  }

  // build the camera's world transform
  vectorial::mat4f view = vectorial::inverse(makeCameraTransform(&s_camera));

  // render
  GL_CHECK(glClearColor(color_val, color_val, color_val, 0.0f));
  GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  GL_CHECK(glViewport(0, 0, (GLsizei)s_window_width, (GLsizei)s_window_height));

  GL_CHECK(glEnable(GL_DEPTH_TEST));
  GL_CHECK(glDepthFunc(GL_LESS));
  GL_CHECK(glEnable(GL_CULL_FACE));
  GL_CHECK(glCullFace(GL_BACK));

  // draw all the models
  draw_models(&s_models[0], (unsigned)s_models.size(), view);

  for (const auto& normal : s_debug_normals) {
    float pos[3] = {normal.p.x, normal.p.y, normal.p.z};
    float nor[3] = {normal.n.x, normal.n.y, normal.n.z};
    float col[3] = {1.0f, 1.0f, 1.0f};
    ddraw_normal(pos, nor, col, 0.5f);
  }
  ddraw_flush();

  if (s_vis_lightmap) {
    // draw the lightmap texture
    draw_debug_texture(s_lightmap_tex_id, -0.8f, -0.8f, 1.6f, 1.6f);
  }

  clear_key_edge_states();
}

extern "C" void app_resize(float width, float height) {
  if (width == 0 || height == 0) {
    return;
  }
  s_window_width = width;
  s_window_height = height;

  GL_CHECK(glViewport(0, 0, (GLsizei)width, (GLsizei)height));
  camera_set_projection(&s_camera, 1.3f, s_window_width, s_window_height);
}
