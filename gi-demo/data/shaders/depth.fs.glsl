#version 330 core

uniform vec2 camera_near_far;

in vec3 f_color;
in vec3 f_position_vs;
in vec3 f_normal_vs;

out vec3 color;

float linearize_depth(float depth) {
  float near = camera_near_far.x;
  float far = camera_near_far.y;
  return (2.0 * near) / (far + near - depth * (far - near));
}

void main() {
  vec3 ignore = f_color * f_position_vs * f_normal_vs;

  float near = camera_near_far.x;
  float far = camera_near_far.y;

  color = vec3(linearize_depth(gl_FragCoord.z));
}
