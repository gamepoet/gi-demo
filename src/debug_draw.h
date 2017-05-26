#pragma once

typedef float DDrawVec3[3];
typedef const float DDrawVec3Param[3];

struct DDrawVertex {
  float pos_x;
  float pos_y;
  float pos_z;
  float col_r;
  float col_g;
  float col_b;
};

struct DDrawSettings {
  int max_points;
  int max_lines;

  void (*draw_points)(const DDrawVertex* vertices, int vertex_count);
  void (*draw_lines)(const DDrawVertex* vertices, int vertex_count);
};

void ddraw_settings_init(DDrawSettings* settings);

void ddraw_init(DDrawSettings* settings);
void ddraw_shutdown();

void ddraw_flush();

void ddraw_point(DDrawVec3Param pos, DDrawVec3Param color);
void ddraw_line(DDrawVec3Param pos0, DDrawVec3Param pos1, DDrawVec3Param color);
void ddraw_line(DDrawVec3Param pos0, DDrawVec3Param pos1, DDrawVec3Param color0, DDrawVec3Param color1);

void ddraw_normal(DDrawVec3Param pos0, DDrawVec3Param normal, DDrawVec3Param color, float length);
