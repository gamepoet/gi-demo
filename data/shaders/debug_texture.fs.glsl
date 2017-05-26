#version 330 core

in vec2 f_uv;

out vec4 color;

uniform sampler2D u_texture;

void main() {
  color = texture(u_texture, f_uv);
}
