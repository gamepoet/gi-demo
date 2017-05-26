#include "debug_draw.h"
#include <stdlib.h>

void (*s_draw_points_func)(const DDrawVertex* vertices, int vertex_count);
void (*s_draw_lines_func)(const DDrawVertex* vertices, int vertex_count);

static DDrawVertex* s_points;
static int s_point_count;
static int s_point_capacity;

static DDrawVertex* s_lines;
static int s_line_count;
static int s_line_capacity;

static void flush_points() {
  if (s_point_count > 0) {
    if (s_draw_points_func) {
      s_draw_points_func(s_points, s_point_count);
    }

    s_point_count = 0;
  }
}

static void flush_lines() {
  if (s_line_count > 0) {
    if (s_draw_lines_func) {
      s_draw_lines_func(s_lines, s_line_count);
    }

    s_line_count = 0;
  }
}

void ddraw_settings_init(DDrawSettings* settings) {
  if (!settings) {
    return;
  }

  settings->max_points = 1024;
  settings->max_lines = 32 * 1024;
  settings->draw_points = nullptr;
  settings->draw_lines = nullptr;
}

void ddraw_init(DDrawSettings* settings) {
  if (!settings) {
    return;
  }

  s_draw_points_func = settings->draw_points;
  s_draw_lines_func = settings->draw_lines;

  s_points = (DDrawVertex*)malloc(settings->max_points * sizeof(DDrawVertex));
  s_point_count = 0;
  s_point_capacity = settings->max_points;

  s_lines = (DDrawVertex*)malloc(settings->max_lines * sizeof(DDrawVertex));
  s_line_count = 0;
  s_line_capacity = settings->max_lines;
}

void ddraw_shutdown() {
  free(s_lines);
  s_lines = nullptr;
  free(s_points);
  s_points = nullptr;
}

void ddraw_flush() {
  flush_points();
  flush_lines();
}

void ddraw_point(DDrawVec3Param pos, DDrawVec3Param color) {
  if (s_point_count + 1 >= s_point_capacity) {
    flush_points();
  }

  s_points[s_point_count] = {
      pos[0], pos[1], pos[2], color[0], color[1], color[2],
  };
}

void ddraw_line(DDrawVec3Param pos0, DDrawVec3Param pos1, DDrawVec3Param color) {
  ddraw_line(pos0, pos1, color, color);
}

void ddraw_line(DDrawVec3Param pos0, DDrawVec3Param pos1, DDrawVec3Param color0, DDrawVec3Param color1) {
  if (s_line_count + 2 >= s_line_capacity) {
    flush_lines();
  }

  s_lines[s_line_count] = {
      pos0[0], pos0[1], pos0[2], color0[0], color0[1], color0[2],
  };

  s_lines[s_line_count + 1] = {
      pos1[0], pos1[1], pos1[2], color1[0], color1[1], color1[2],
  };
  s_line_count += 2;
}

void ddraw_normal(DDrawVec3Param pos, DDrawVec3Param normal, DDrawVec3Param color, float length) {
  DDrawVec3 pos1;
  pos1[0] = pos[0] + (normal[0] * length);
  pos1[1] = pos[1] + (normal[1] * length);
  pos1[2] = pos[2] + (normal[2] * length);

  ddraw_line(pos, pos1, color);
}
