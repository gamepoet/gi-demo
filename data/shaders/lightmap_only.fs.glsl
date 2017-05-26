#version 330 core

uniform sampler2D u_texture_lightmap;

in vec2 f_lightmap_uv;

out vec4 color;

void main() {
  color = texture(u_texture_lightmap, f_lightmap_uv);
}
