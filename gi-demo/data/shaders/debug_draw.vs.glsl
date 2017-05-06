#version 330 core
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_color;

out vec3 f_color;

uniform mat4 world_view_proj;

void main() {
  gl_Position = world_view_proj * vec4(v_position, 1.0);

  f_color = v_color;
}
