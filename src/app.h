#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AppKeyCode {
  APP_KEY_CODE_A,
  APP_KEY_CODE_D,
  APP_KEY_CODE_E,
  APP_KEY_CODE_Q,
  APP_KEY_CODE_R,
  APP_KEY_CODE_S,
  APP_KEY_CODE_W,
  APP_KEY_CODE_F1,
  APP_KEY_CODE_F2,
  APP_KEY_CODE_F3,
  APP_KEY_CODE_F4,
  APP_KEY_CODE_F5,
  APP_KEY_CODE_F6,
  APP_KEY_CODE_F7,
  APP_KEY_CODE_F8,
  APP_KEY_CODE_F9,
  APP_KEY_CODE_F10,
  APP_KEY_CODE_F11,
  APP_KEY_CODE_F12,
  APP_KEY_CODE_UP,
  APP_KEY_CODE_DOWN,
  APP_KEY_CODE_LEFT,
  APP_KEY_CODE_RIGHT,
  APP_KEY_CODE_LALT,
  APP_KEY_CODE_LCONTROL,
  APP_KEY_CODE_LSHIFT,
  APP_KEY_CODE_RALT,
  APP_KEY_CODE_RCONTROL,
  APP_KEY_CODE_RSHIFT,
  APP_KEY_CODE_MINUS,
  APP_KEY_CODE_EQUAL,
  // ...
  APP_KEY_CODE_COUNT
} AppKeyCode;

void app_render(float dt);
void app_resize(float width, float height);

void app_input_key_down(AppKeyCode key);
void app_input_key_up(AppKeyCode key);

#ifdef __cplusplus
}
#endif
