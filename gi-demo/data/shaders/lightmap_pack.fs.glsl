#version 330 core

uniform vec4 u_color;

out vec3 color;

void main() {
  color = u_color.rgb;
}
