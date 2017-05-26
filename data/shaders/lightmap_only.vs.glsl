#version 330 core
layout(location = 0) in vec3 v_position;
layout(location = 15) in vec2 v_lightmap_uv;

out vec2 f_lightmap_uv;

uniform mat4 world_view_proj;

void main() {
  gl_Position = world_view_proj * vec4(v_position, 1.0);
  f_lightmap_uv = v_lightmap_uv;
}
