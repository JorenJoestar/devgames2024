/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "input.hpp"
#include "kernel/log.hpp"
#include "kernel/assert.hpp"

#if defined(_MSC_VER)
#include <Windows.h>
#endif

#include <cmath>

#include <SDL.h>

#include "imgui.h"

namespace idra {
    
static cstring s_key_names[] = {
"unknown",
"uuuu0",
"uuuu1",
"uuuu2",
"a",
"b",
"c",
"d",
"e",
"f",
"g",
"h",
"i",
"j",
"k",
"l",
"m",
"n",
"o",
"p",
"q",
"r",
"s",
"t",
"u",
"v",
"w",
"x",
"y",
"z",
"1",
"2",
"3",
"4",
"5",
"6",
"7",
"8",
"9",
"0",
"return",
"escape",
"backspace",
"tab",
"space",
"minus",
"equals",
"leftbracket",
"rightbracket",
"backslash",
"nonushash",
"semicolon",
"apostrophe",
"grave",
"comma",
"period",
"slash",
"capslock",
"f1",
"f2",
"f3",
"f4",
"f5",
"f6",
"f7",
"f8",
"f9",
"f10",
"f11",
"f12",
"printscreen",
"scrolllock",
"pause",
"insert",
"home",
"pageup",
"delete",
"end",
"pagedown",
"right",
"left",
"down",
"up",
"numlock",
"kp_divide",
"kp_multiply",
"kp_minus",
"kp_plus",
"kp_enter",
"kp_1",
"kp_2",
"kp_3",
"kp_4",
"kp_5",
"kp_6",
"kp_7",
"kp_8",
"kp_9",
"kp_0",
"kp_period",
"nonusbackslash",
"application",
"power",
"kp_equals",
"f13",
"f14",
"f15",
"f16",
"f17",
"f18",
"f19",
"f20",
"f21",
"f22",
"f23",
"f24",
"exe",
"help",
"menu",
"select",
"stop",
"again",
"undo",
"cut",
"copy",
"paste",
"find",
"mute",
"volumeup",
"volumedown",
"130",
"131",
"132",
"kp_comma",
"kp_equalsas400",
"international1",
"international2",
"international3",
"international4",
"international5",
"international6",
"international7",
"international8",
"international9",
"lang1",
"lang2",
"lang3",
"lang4",
"lang5",
"lang6",
"lang7",
"lang8",
"lang9",
"alterase",
"sysreq",
"cancel",
"clear",
"prior",
"return2",
"separator",
"out",
"oper",
"clearagain",
"crsel",
"exsel",
"plus",
"166",
"167",
"168",
"169",
"170",
"171",
"172",
"173",
"174",
"175",
"kp_00",
"kp_000",
"thousandsseparator",
"decimalseparator",
"currencyunit",
"currencysubunit",
"kp_leftparen",
"kp_rightparen",
"kp_leftbrace",
"kp_rightbrace",
"kp_tab",
"kp_backspace",
"kp_a",
"kp_b",
"kp_c",
"kp_d",
"kp_e",
"kp_f",
"kp_xor",
"kp_power",
"kp_percent",
"kp_less",
"kp_greater",
"kp_ampersand",
"kp_dblampersand",
"kp_verticalbar",
"kp_dblverticalbar",
"kp_colon",
"kp_hash",
"kp_space",
"kp_at",
"kp_exclam",
"kp_memstore",
"kp_memrecall",
"kp_memclear",
"kp_memadd",
"kp_memsubtract",
"kp_memmultiply",
"kp_memdivide",
"kp_plusminus",
"kp_clear",
"kp_clearentry",
"kp_binary",
"kp_octal",
"kp_decimal",
"kp_hexadecimal",
"222",
"223",
"lctrl",
"lshift",
"lalt",
"lgui",
"rctrl",
"rshift",
"ralt",
"rgui",
"tilde",

"mode",
"audionext",
"audioprev",
"audiostop",
"audioplay",
"audiomute",
"mediaselect",
"www",
"mail",
"calculator",
"computer",
"ac_search",
"ac_home",
"ac_back",
"ac_forward",
"ac_stop",
"ac_refresh",
"ac_bookmarks",
"brightnessdown",
"brightnessup",
"displayswitch",
"kbdillumtoggle",
"kbdillumdown",
"kbdillumup",
"eject",
"sleep",
"app1",
"app2",
"audiorewind",
"audiofastforward",

}; // s_key_names

cstring* key_names() {
    return s_key_names;
}

#if defined(_MSC_VER)
Keys key_translate( WPARAM key ) {
    switch ( key )
    {
        case VK_BACK:
            return KEY_BACKSPACE;
        case VK_TAB:
            return KEY_TAB;
        case VK_CLEAR:
            return KEY_CLEAR;
        case VK_RETURN:
            return KEY_RETURN;
        case VK_PAUSE:
            return KEY_PAUSE;
        case VK_ESCAPE:
            return KEY_ESCAPE;
        case VK_SPACE:
            return KEY_SPACE;
        case VK_OEM_PLUS:
            return KEY_PLUS;
        case VK_OEM_COMMA:
            return KEY_COMMA;
        case VK_OEM_MINUS:
            return KEY_MINUS;
        case VK_OEM_PERIOD:
            return KEY_PERIOD;
        case VK_OEM_1:
            return KEY_SEMICOLON;
        case VK_OEM_2:
            return KEY_SLASH;
        case VK_OEM_3:
            return KEY_TILDE;
        case VK_OEM_4:
            return KEY_LEFTBRACKET;
        case VK_OEM_5:
            return KEY_BACKSLASH;
        case VK_OEM_6:
            return KEY_RIGHTBRACKET;
        case VK_OEM_7:
            return KEY_APOSTROPHE;
        case VK_OEM_8:
            return KEY_UNKNOWN;
        case 48:
            return KEY_0;
        case 49:
            return KEY_1;
        case 50:
            return KEY_2;
        case 51:
            return KEY_3;
        case 52:
            return KEY_4;
        case 53:
            return KEY_5;
        case 54:
            return KEY_6;
        case 55:
            return KEY_7;
        case 56:
            return KEY_8;
        case 57:
            return KEY_9;
        case 65:
            return KEY_A;
        case 66:
            return KEY_B;
        case 67:
            return KEY_C;
        case 68:
            return KEY_D;
        case 69:
            return KEY_E;
        case 70:
            return KEY_F;
        case 71:
            return KEY_G;
        case 72:
            return KEY_H;
        case 73:
            return KEY_I;
        case 74:
            return KEY_J;
        case 75:
            return KEY_K;
        case 76:
            return KEY_L;
        case 77:
            return KEY_M;
        case 78:
            return KEY_N;
        case 79:
            return KEY_O;
        case 80:
            return KEY_P;
        case 81:
            return KEY_Q;
        case 82:
            return KEY_R;
        case 83:
            return KEY_S;
        case 84:
            return KEY_T;
        case 85:
            return KEY_U;
        case 86:
            return KEY_V;
        case 87:
            return KEY_W;
        case 88:
            return KEY_X;
        case 89:
            return KEY_Y;
        case 90:
            return KEY_Z;
        case VK_DELETE:
            return KEY_DELETE;
        case VK_NUMPAD0:
            return KEY_KP_0;
        case VK_NUMPAD1:
            return KEY_KP_1;
        case VK_NUMPAD2:
            return KEY_KP_2;
        case VK_NUMPAD3:
            return KEY_KP_3;
        case VK_NUMPAD4:
            return KEY_KP_4;
        case VK_NUMPAD5:
            return KEY_KP_5;
        case VK_NUMPAD6:
            return KEY_KP_6;
        case VK_NUMPAD7:
            return KEY_KP_7;
        case VK_NUMPAD8:
            return KEY_KP_8;
        case VK_NUMPAD9:
            return KEY_KP_9;
        case VK_DECIMAL:
            return KEY_KP_PERIOD;
        case VK_DIVIDE:
            return KEY_KP_DIVIDE;
        case VK_MULTIPLY:
            return KEY_KP_MULTIPLY;
        case VK_SUBTRACT:
            return KEY_KP_MINUS;
        case VK_ADD:
            return KEY_KP_PLUS;
        case VK_UP:
            return KEY_UP;
        case VK_DOWN:
            return KEY_DOWN;
        case VK_RIGHT:
            return KEY_RIGHT;
        case VK_LEFT:
            return KEY_LEFT;
        case VK_INSERT:
            return KEY_INSERT;
        case VK_HOME:
            return KEY_HOME;
        case VK_END:
            return KEY_END;
        case VK_PRIOR:
            return KEY_PAGEUP;
        case VK_NEXT:
            return KEY_PAGEDOWN;
        case VK_F1:
            return KEY_F1;
        case VK_F2:
            return KEY_F2;
        case VK_F3:
            return KEY_F3;
        case VK_F4:
            return KEY_F4;
        case VK_F5:
            return KEY_F5;
        case VK_F6:
            return KEY_F6;
        case VK_F7:
            return KEY_F7;
        case VK_F8:
            return KEY_F8;
        case VK_F9:
            return KEY_F9;
        case VK_F10:
            return KEY_F10;
        case VK_F11:
            return KEY_F11;
        case VK_F12:
            return KEY_F12;
        case VK_NUMLOCK:
            return KEY_NUMLOCK;
        case VK_SCROLL:
            return KEY_SCROLLLOCK;
        case VK_SHIFT:
            return KEY_LSHIFT;
        case VK_CONTROL:
            return KEY_RCTRL;
        case VK_RSHIFT:
            return KEY_RSHIFT;
        case VK_LSHIFT:
            return KEY_LSHIFT;
        case VK_RCONTROL:
            return KEY_RCTRL;
        case VK_LCONTROL:
            return KEY_LCTRL;
        case VK_LMENU:
            return KEY_LALT;
        case VK_RMENU:
            return KEY_RALT;
    }
    return KEY_UNKNOWN;
}
#endif // _MSC_VER

// InputSystem ////////////////////////////////////////////////////////////
static InputSystem s_input_system;

InputSystem* InputSystem::init_system() {

    memset( s_input_system.keyboard_current.keys, 0, ArraySize( s_input_system.keyboard_current.keys ) );
    memset( s_input_system.keyboard_previous.keys, 0, ArraySize( s_input_system.keyboard_previous.keys ) );

    if ( SDL_WasInit( SDL_INIT_GAMECONTROLLER ) != 1 )
        SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER );

    SDL_GameControllerEventState( SDL_ENABLE );

    for ( u32 i = 0; i < k_max_gamepads; i++ ) {
        s_input_system.gamepad_current[ i ].index = u32_max;
        s_input_system.gamepad_current[ i ].id = u32_max;

        s_input_system.gamepad_previous[ i ].index = u32_max;
        s_input_system.gamepad_previous[ i ].id = u32_max;
    }

    const i32 num_joystics = SDL_NumJoysticks();
    if ( num_joystics > 0 ) {

        ilog_debug( "Detected joysticks!" );

        for ( i32 i = 0; i < num_joystics; i++ ) {
            if ( SDL_IsGameController( i ) ) {
                s_input_system.init_gamepad( i );
            }
        }
    }

    return &s_input_system;
}

void InputSystem::shutdown_system( InputSystem* system ) {
    iassert( system == &s_input_system );

    SDL_GameControllerEventState( SDL_DISABLE );
}


static u32 to_sdl_mouse_button( MouseButtons button ) {
    switch ( button ) {
        case MOUSE_BUTTONS_LEFT:
            return SDL_BUTTON_LEFT;
        case MOUSE_BUTTONS_MIDDLE:
            return SDL_BUTTON_MIDDLE;
        case MOUSE_BUTTONS_RIGHT:
            return SDL_BUTTON_RIGHT;
    }

    return u32_max;
}

static void get_mouse_state( MouseState& mouse_state ) {
    i32 mouse_x, mouse_y;
    u32 mouse_buttons = SDL_GetMouseState( &mouse_x, &mouse_y );

    for ( u32 i = 0; i < MOUSE_BUTTONS_COUNT; ++i ) {
        u32 sdl_button = to_sdl_mouse_button( ( MouseButtons )i );
        mouse_state.buttons[ i ] = mouse_buttons & SDL_BUTTON( sdl_button );
    }
    
    mouse_state.position[ 0 ] = ( i16 )mouse_x;
    mouse_state.position[ 1 ] = ( i16 )mouse_y;
}

void InputSystem::update() {

    // Update previous devices with the ones from previous frame
    memcpy(keyboard_previous.keys, keyboard_current.keys, ArraySize( keyboard_previous.keys ) );
    memcpy( &mouse_previous, &mouse_current, sizeof( MouseState ) );
    memcpy( &gamepad_previous[ 0 ], &gamepad_current[ 0 ], sizeof( Gamepad ) * k_max_gamepads );

    get_mouse_state( mouse_current );

    for ( u32 i = 0; i < MOUSE_BUTTONS_COUNT; ++i ) {
        // Just clicked. Save position
        if ( is_mouse_clicked( ( MouseButtons )i ) ) {
            // TODO
            //mouse_clicked_position[ i ] = mouse_position;
            mouse_current.clicked_position[ i ][ 0 ] = mouse_current.position[ 0 ];
            mouse_current.clicked_position[ i ][ 1 ] = mouse_current.position[ 1 ];
            //ilog( "%d %d\n", mouse_current.position[ 0 ], mouse_current.position[ 1 ] );
        } else if ( is_mouse_down( ( MouseButtons )i ) ) {
            f32 delta_x = mouse_current.position[ 0 ] * 1.0f - mouse_current.clicked_position[ i ][ 0 ];
            f32 delta_y = mouse_current.position[ 1 ] * 1.0f - mouse_current.clicked_position[ i ][ 1 ];
            mouse_current.drag_distance[ i ] = sqrtf( ( delta_x * delta_x ) + ( delta_y * delta_y ) );

            //ilog( "%d %d\n", mouse_current.clicked_position[i][ 0 ], mouse_current.clicked_position[i][ 1 ] );
        }
    }
}

static constexpr f32 k_mouse_drag_min_distance = 4.f;

bool InputSystem::is_key_down( Keys key ) {
    return keyboard_current.keys[ key ] && has_focus;
}

bool InputSystem::is_key_just_pressed( Keys key, bool repeat ) {
    return keyboard_current.keys[ key ] && !keyboard_previous.keys[ key ] && has_focus;
}

bool InputSystem::is_key_just_released( Keys key ) {
    return !keyboard_current.keys[ key ] && keyboard_previous.keys[ key ] && has_focus;
}

bool InputSystem::is_mouse_down( MouseButtons button ) {
    return mouse_current.buttons[ button ];
}

bool InputSystem::is_mouse_clicked( MouseButtons button ) {
    return mouse_current.buttons[ button ] && !mouse_previous.buttons[ button ];
}

bool InputSystem::is_mouse_released( MouseButtons button ) {
    return !mouse_current.buttons[ button ];
}

bool InputSystem::is_mouse_dragging( MouseButtons button ) {
    if ( !mouse_current.buttons[ button ] )
        return false;

    return mouse_current.drag_distance[ button ] > k_mouse_drag_min_distance;
}

static bool sdl_init_gamepad( int32_t index, Gamepad& gamepad ) {

    SDL_GameController* pad = SDL_GameControllerOpen( index );
    //SDL_Joystick* joy = SDL_JoystickOpen( index );

    // Set memory to 0
    memset( &gamepad, 0, sizeof( Gamepad ) );

    if ( pad ) {
        ilog_debug( "Opened Joystick 0\n" );
        ilog_debug( "Name: %s\n", SDL_GameControllerNameForIndex( index ) );
        //hprint( "Number of Axes: %d\n", SDL_JoystickNumAxes( joy ) );
        //hprint( "Number of Buttons: %d\n", SDL_JoystickNumButtons( joy ) );

        SDL_Joystick* joy = SDL_GameControllerGetJoystick( pad );

        gamepad.index = index;
        gamepad.name = SDL_JoystickNameForIndex( index );
        gamepad.handle = pad;
        gamepad.id = SDL_JoystickInstanceID( joy );

        return true;

    } else {
        ilog_debug( "Couldn't open Joystick %u\n", index );
        gamepad.index = u32_max;

        return false;
    }
}

static void sdl_shutdown_gamepad( Gamepad& gamepad ) {

    SDL_JoystickClose( ( SDL_Joystick* )gamepad.handle );
    gamepad.index = u32_max;
    gamepad.name = { nullptr, 0 };
    gamepad.handle = 0;
    gamepad.id = u32_max;
}

void InputSystem::init_gamepad( u32 index ) {

    sdl_init_gamepad( index, gamepad_current[ index ] );
}

void InputSystem::shutdown_gamepad( u32 index ) {

    if ( index >= k_max_gamepads ) {
        ilog_warn( "Trying to shutdown gamepad %u, but index is invalid. \n", index );
        return;
    }

    // Search for the correct gamepad
    for ( u32 i = 0; i < k_max_gamepads; i++ ) {
        if ( gamepad_current[ i ].id == index ) {
            sdl_shutdown_gamepad( gamepad_current[ i ] );
            break;
        }
    }
}

void InputSystem::set_gamepad_axis_value( u32 index, GamepadAxis axis, f32 value ) {

    for ( size_t i = 0; i < k_max_gamepads; i++ ) {
        if ( gamepad_current[ i ].id == index ) {
            gamepad_current[ i ].axis[ axis ] = value;
            break;
        }
    }
}

void InputSystem::set_gamepad_button( u32 index, GamepadButtons button, u8 state ) {

    for ( size_t i = 0; i < k_max_gamepads; i++ ) {
        if ( gamepad_current[ i ].id == index ) {
            gamepad_current[ i ].buttons[ button ] = state;
            break;
        }
    }
}

bool InputSystem::is_gamepad_attached( u32 index ) const {
    if ( index >= k_max_gamepads ) {
        return false;
    }
    return gamepad_current[ index ].id >= 0;
}

bool InputSystem::is_gamepad_button_down( u32 index, GamepadButtons button ) const {
    if ( index >= k_max_gamepads ) {
        return false;
    }
    return gamepad_current[ index ].buttons[ button ] == 1;
}

bool InputSystem::is_gamepad_button_just_pressed( u32 index, GamepadButtons button ) const {
    if ( index >= k_max_gamepads ) {
        return false;
    }
    return (gamepad_current[ index ].buttons[ button ] == 1) && ( gamepad_previous[ index ].buttons[ button ] == 0);
}

f32 InputSystem::get_gamepad_axis_value( u32 index, GamepadAxis axis ) const {
    if ( index >= k_max_gamepads ) {
        return 0.0f;
    }
    return gamepad_current[ index ].axis[ axis ];
}


cstring* gamepad_axis_names() {
    static cstring names[] = { "left_x", "left_y", "right_x", "right_y", "trigger_left", "trigger_right", "gamepad_axis_count" };
    return names;
}

cstring* gamepad_button_names() {
    static cstring names[] = { "a", "b", "x", "y", "back", "guide", "start", "left_stick", "right_stick", "left_shoulder", "right_shoulder", "dpad_up", "dpad_down", "dpad_left", "dpad_right", "gamepad_button_count", };
    return names;
}

cstring* mouse_button_names() {
    static cstring names[] = { "left", "right", "middle", "mouse_button_count", };
    return names;
}

void InputSystem::debug_ui() {

    if ( ImGui::Begin( "Input" ) ) {
        ImGui::Text( "Has focus %u", has_focus ? 1 : 0 );

        if ( ImGui::TreeNode( "Devices" ) ) {
            ImGui::Separator();
            if ( ImGui::TreeNode( "Gamepads" ) ) {
                for ( u32 i = 0; i < k_max_gamepads; ++i ) {
                    const Gamepad& g = gamepad_current[ i ];
                    ImGui::Text( "Name: %s, id %d, index %u", g.name.data, g.id, g.index );
                    // Attached gamepad
                    if ( is_gamepad_attached( i ) ) {
                        ImGui::NewLine();
                        ImGui::Columns( GAMEPAD_AXIS_COUNT );
                        for ( u32 gi = 0; gi < GAMEPAD_AXIS_COUNT; gi++ ) {
                            ImGui::Text( "%s", gamepad_axis_names()[ gi ] );
                            ImGui::NextColumn();
                        }
                        for ( u32 gi = 0; gi < GAMEPAD_AXIS_COUNT; gi++ ) {
                            ImGui::Text( "%f", g.axis[ gi ] );
                            ImGui::NextColumn();
                        }
                        ImGui::NewLine();
                        ImGui::Columns( GAMEPAD_BUTTON_COUNT );
                        for ( u32 gi = 0; gi < GAMEPAD_BUTTON_COUNT; gi++ ) {
                            ImGui::Text( "%s", gamepad_button_names()[ gi ] );
                            ImGui::NextColumn();
                        }
                        ImGui::Columns( GAMEPAD_BUTTON_COUNT );
                        for ( u32 gi = 0; gi < GAMEPAD_BUTTON_COUNT; gi++ ) {
                            ImGui::Text( "%u", g.buttons[ gi ] );
                            ImGui::NextColumn();
                        }

                        ImGui::Columns( 1 );
                    }
                    ImGui::Separator();
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            if ( ImGui::TreeNode( "Mouse" ) ) {
                ImGui::Text( "Position     %d,%d", mouse_current.position[ 0 ], mouse_current.position[ 1 ] );
                ImGui::Text( "Previous pos %d,%d", mouse_previous.position[ 0 ], mouse_previous.position[ 1 ] );

                ImGui::Separator();

                for ( u32 i = 0; i < MOUSE_BUTTONS_COUNT; i++ ) {
                    ImGui::Text( "Button %u", i );
                    ImGui::SameLine();
                    ImGui::Text( "Clicked Position     %d,%d", mouse_current.clicked_position[ i ][ 0 ], mouse_current.clicked_position[ i ][ 1 ] );
                    ImGui::SameLine();
                    ImGui::Text( "Button %u, Previous %u", mouse_current.buttons[ i ], mouse_previous.buttons[ i ] );
                    ImGui::SameLine();
                    ImGui::Text( "Drag %f", mouse_current.drag_distance[ i ] );

                    ImGui::Separator();
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            if ( ImGui::TreeNode( "Keyboard" ) ) {
                for ( u32 i = 0; i < KEY_LAST; i++ ) {

                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        /*if ( ImGui::TreeNode( "Actions" ) ) {

            for ( u32 j = 0; j < actions.size; j++ ) {
                const InputAction& input_action = actions[ j ];
                ImGui::Text( "Action %s, x %2.3f y %2.3f", input_action.name, input_action.value.x, input_action.value.y );
            }

            ImGui::TreePop();
        }

        if ( ImGui::TreeNode( "Bindings" ) ) {
            for ( u32 k = 0; k < bindings.size; k++ ) {
                const InputBinding& binding = bindings[ k ];
                const InputAction& parent_action = actions[ binding.action_index ];

                cstring button_name = "";
                switch ( binding.device_part ) {
                    case DEVICE_PART_KEYBOARD:
                    {
                        button_name = key_names()[ binding.button ];
                        break;
                    }
                    case DEVICE_PART_MOUSE:
                    {
                        break;
                    }
                    case DEVICE_PART_GAMEPAD_AXIS:
                    {
                        break;
                    }
                    case DEVICE_PART_GAMEPAD_BUTTONS:
                    {
                        break;
                    }
                }

                switch ( binding.type ) {
                    case BINDING_TYPE_VECTOR_1D:
                    {
                        ImGui::Text( "Binding action %s, type %s, value %f, composite %u, part of composite %u, button %s", parent_action.name, "vector 1d", binding.value, binding.is_composite, binding.is_part_of_composite, button_name );
                        break;
                    }
                    case BINDING_TYPE_VECTOR_2D:
                    {
                        ImGui::Text( "Binding action %s, type %s, value %f, composite %u, part of composite %u", parent_action.name, "vector 2d", binding.value, binding.is_composite, binding.is_part_of_composite );
                        break;
                    }
                    case BINDING_TYPE_AXIS_1D:
                    {
                        ImGui::Text( "Binding action %s, type %s, value %f, composite %u, part of composite %u", parent_action.name, "axis 1d", binding.value, binding.is_composite, binding.is_part_of_composite );
                        break;
                    }
                    case BINDING_TYPE_AXIS_2D:
                    {
                        ImGui::Text( "Binding action %s, type %s, value %f, composite %u, part of composite %u", parent_action.name, "axis 2d", binding.value, binding.is_composite, binding.is_part_of_composite );
                        break;
                    }
                    case BINDING_TYPE_BUTTON:
                    {
                        ImGui::Text( "Binding action %s, type %s, value %f, composite %u, part of composite %u, button %s", parent_action.name, "button", binding.value, binding.is_composite, binding.is_part_of_composite, button_name );
                        break;
                    }
                }
            }

            ImGui::TreePop();
        }*/

    }
    ImGui::End();
}


} // namespace input