#version 330 core
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec3 v_color;

out vec3 f_position_vs;
out vec3 f_normal_vs;
out vec3 f_color;

uniform mat4 world_view_proj;
uniform mat4 world_view;

void main() {
  gl_Position = world_view_proj * vec4(v_position, 1.0);

  f_position_vs = vec3(world_view * vec4(v_position, 1.0));
  f_normal_vs = mat3(world_view) * v_normal;
  f_color = v_color;
}
