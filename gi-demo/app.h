#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AppKeyCode {
  APP_KEY_CODE_A,
  APP_KEY_CODE_D,
  APP_KEY_CODE_E,
  APP_KEY_CODE_Q,
  APP_KEY_CODE_S,
  APP_KEY_CODE_W,
  APP_KEY_CODE_UP,
  APP_KEY_CODE_DOWN,
  APP_KEY_CODE_LEFT,
  APP_KEY_CODE_RIGHT,
  // ...
  APP_KEY_CODE_COUNT
} AppKeyCode;

void app_render(float dt);

void app_input_key_down(AppKeyCode key);
void app_input_key_up(AppKeyCode key);

#ifdef __cplusplus
}
#endif
