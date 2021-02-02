/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "sdl_emb.h"
#include "../video/sdlvid.h"
#include <Limelight.h>

#define ACTION_MODIFIERS (MODIFIER_SHIFT | MODIFIER_ALT | MODIFIER_CTRL)
#define QUIT_KEY SDLK_q
#define QUIT_BUTTONS (PLAY_FLAG | BACK_FLAG | LB_FLAG | RB_FLAG)
#define FULLSCREEN_KEY SDLK_f

typedef struct _GAMEPAD_STATE
{
  char leftTrigger, rightTrigger;
  short leftStickX, leftStickY;
  short rightStickX, rightStickY;
  int buttons;
  SDL_JoystickID sdl_id;
  SDL_Haptic *haptic;
  int haptic_effect_id;
  short id;
  bool initialized;
} GAMEPAD_STATE, *PGAMEPAD_STATE;

static GAMEPAD_STATE gamepads[4];

static int keyboard_modifiers;
static int activeGamepadMask = 0;

int sdl_gamepads = 0;

static PGAMEPAD_STATE get_gamepad(SDL_JoystickID sdl_id)
{
  for (int i = 0; i < 4; i++)
  {
    if (!gamepads[i].initialized)
    {
      gamepads[i].sdl_id = sdl_id;
      gamepads[i].id = i;
      gamepads[i].initialized = true;
      activeGamepadMask |= (1 << i);
      return &gamepads[i];
    }
    else if (gamepads[i].sdl_id == sdl_id)
      return &gamepads[i];
  }
  return &gamepads[0];
}

int sdlinput_handle_event(SDL_Event *event)
{
  int button = 0;
  PGAMEPAD_STATE gamepad;
  switch (event->type)
  {
  case SDL_MOUSEMOTION:
    LiSendMouseMoveEvent(event->motion.xrel, event->motion.yrel);
    break;
  case SDL_MOUSEWHEEL:
    LiSendScrollEvent(event->wheel.y);
    break;
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEBUTTONDOWN:
    switch (event->button.button)
    {
    case SDL_BUTTON_LEFT:
      button = BUTTON_LEFT;
      break;
    case SDL_BUTTON_MIDDLE:
      button = BUTTON_MIDDLE;
      break;
    case SDL_BUTTON_RIGHT:
      button = BUTTON_RIGHT;
      break;
    case SDL_BUTTON_X1:
      button = BUTTON_X1;
      break;
    case SDL_BUTTON_X2:
      button = BUTTON_X2;
      break;
    }

    if (button != 0)
      LiSendMouseButtonEvent(event->type == SDL_MOUSEBUTTONDOWN ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, button);

    return SDL_MOUSE_GRAB;
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    button = event->key.keysym.sym;
    if (button >= 0x21 && button <= 0x2f)
      button = keyCodes1[button - 0x21];
    else if (button >= 0x3a && button <= 0x40)
      button = keyCodes2[button - 0x3a];
    else if (button >= 0x5b && button <= 0x60)
      button = keyCodes3[button - 0x5b];
    else if (button >= 0x40000039 && button < 0x40000039 + sizeof(keyCodes4))
      button = keyCodes4[button - 0x40000039];
    else if (button >= 0x400000E0 && button <= 0x400000E7)
      button = keyCodes5[button - 0x400000E0];
    else if (button >= 0x61 && button <= 0x7a)
      button -= 0x20;
    else if (button == 0x7f)
      button = 0x2e;

    int modifier = 0;
    switch (event->key.keysym.sym)
    {
    case SDLK_RSHIFT:
    case SDLK_LSHIFT:
      modifier = MODIFIER_SHIFT;
      break;
    case SDLK_RALT:
    case SDLK_LALT:
      modifier = MODIFIER_ALT;
      break;
    case SDLK_RCTRL:
    case SDLK_LCTRL:
      modifier = MODIFIER_CTRL;
      break;
    }

    if (modifier != 0)
    {
      if (event->type == SDL_KEYDOWN)
        keyboard_modifiers |= modifier;
      else
        keyboard_modifiers &= ~modifier;
    }

    // Quit the stream if all the required quit keys are down
    if ((keyboard_modifiers & ACTION_MODIFIERS) == ACTION_MODIFIERS && event->key.keysym.sym == QUIT_KEY && event->type == SDL_KEYUP)
      return SDL_QUIT_APPLICATION;
    else if ((keyboard_modifiers & ACTION_MODIFIERS) == ACTION_MODIFIERS && event->key.keysym.sym == FULLSCREEN_KEY && event->type == SDL_KEYUP)
      return SDL_TOGGLE_FULLSCREEN;
    else if ((keyboard_modifiers & ACTION_MODIFIERS) == ACTION_MODIFIERS)
      return SDL_MOUSE_UNGRAB;

    LiSendKeyboardEvent(0x80 << 8 | button, event->type == SDL_KEYDOWN ? KEY_ACTION_DOWN : KEY_ACTION_UP, keyboard_modifiers);
    break;
  case SDL_CONTROLLERAXISMOTION:
    gamepad = get_gamepad(event->caxis.which);
    switch (event->caxis.axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
      gamepad->leftStickX = event->caxis.value;
      break;
    case SDL_CONTROLLER_AXIS_LEFTY:
      gamepad->leftStickY = -event->caxis.value - 1;
      break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
      gamepad->rightStickX = event->caxis.value;
      break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
      gamepad->rightStickY = -event->caxis.value - 1;
      break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
      gamepad->leftTrigger = (event->caxis.value >> 8) + 127;
      break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
      gamepad->rightTrigger = (event->caxis.value >> 8) + 127;
      break;
    default:
      return SDL_NOTHING;
    }
    LiSendMultiControllerEvent(gamepad->id, activeGamepadMask, gamepad->buttons, gamepad->leftTrigger, gamepad->rightTrigger, gamepad->leftStickX, gamepad->leftStickY, gamepad->rightStickX, gamepad->rightStickY);
    break;
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
    gamepad = get_gamepad(event->cbutton.which);
    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_A:
      button = A_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_B:
      button = B_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_Y:
      button = Y_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_X:
      button = X_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      button = UP_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      button = DOWN_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      button = RIGHT_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      button = LEFT_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      button = BACK_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_START:
      button = PLAY_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_GUIDE:
      button = SPECIAL_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
      button = LS_CLK_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
      button = RS_CLK_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      button = LB_FLAG;
      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      button = RB_FLAG;
      break;
    default:
      return SDL_NOTHING;
    }
    if (event->type == SDL_CONTROLLERBUTTONDOWN)
      gamepad->buttons |= button;
    else
      gamepad->buttons &= ~button;

    if ((gamepad->buttons & QUIT_BUTTONS) == QUIT_BUTTONS)
      return SDL_QUIT_APPLICATION;

    LiSendMultiControllerEvent(gamepad->id, activeGamepadMask, gamepad->buttons, gamepad->leftTrigger, gamepad->rightTrigger, gamepad->leftStickX, gamepad->leftStickY, gamepad->rightStickX, gamepad->rightStickY);
    break;
  }
  return SDL_NOTHING;
}