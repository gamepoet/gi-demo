#version 330 core

uniform vec3 light_pos_vs;
uniform vec3 light_color;
uniform float light_intensity;
uniform float light_range;

in vec3 f_color;
in vec3 f_position_vs;
in vec3 f_normal_vs;

out vec3 color;

void main() {
  vec3 albedo = f_color;

  vec3 n = f_normal_vs;
  vec3 l = light_pos_vs - f_position_vs;
  float l_dist = length(l);
  l = l / l_dist;

  float attenuation = 1.0 - smoothstep(light_range * 0.75, light_range, l_dist);

  float n_dot_l = clamp(dot(n, l), 0, 1);
  vec3 diffuse = n_dot_l * light_color * attenuation * light_intensity;

  color = vec3(0.1f) * albedo + albedo * diffuse;
}
