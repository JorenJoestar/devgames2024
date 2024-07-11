/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "window.hpp"
#include "kernel/log.hpp"
#include "kernel/assert.hpp"
#include "kernel/input.hpp"
#include "kernel/numerics.hpp"

#if defined(_MSC_VER)

#include <windows.h>
#include <strsafe.h>

#endif // _MSC_VER

#include <SDL.h>
#include <SDL_vulkan.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"

static SDL_Window* window = nullptr;

namespace idra {

// Windows procedure callback. 
//LRESULT CALLBACK window_procedure( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );


#if defined(_MSC_VER)
void print_last_error( LPCSTR lpszFunction, DWORD error ) {
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        ( LPTSTR )&lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = ( LPVOID )LocalAlloc( LMEM_ZEROINIT,
                                         ( lstrlen( ( LPCTSTR )lpMsgBuf ) + lstrlen( ( LPCTSTR )lpszFunction ) + 40 ) * sizeof( TCHAR ) );
    StringCchPrintf( ( LPTSTR )lpDisplayBuf,
                     LocalSize( lpDisplayBuf ) / sizeof( TCHAR ),
                     TEXT( "%s failed with error %d: %s" ),
                     lpszFunction, error, lpMsgBuf );
    ilog_error( ( LPCTSTR )lpDisplayBuf );

    LocalFree( lpMsgBuf );
    LocalFree( lpDisplayBuf );
}

void print_last_error( LPCSTR lpszFunction ) {
    DWORD dw = GetLastError();

    print_last_error( lpszFunction, dw );
}
#endif  // _MSC_VER

void Window::init( u32 width_, u32 height_, StringView name, Allocator* allocator, InputSystem* input_ ) {

    is_running = false;

    input = input_;

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        ilog_error( "SDL Init error: %s\n", SDL_GetError() );
        return;
    }

    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode( 0, &current );

    SDL_WindowFlags window_flags = ( SDL_WindowFlags )( SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI );

    window = SDL_CreateWindow( name.data, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width_, height_, window_flags );

    ilog_debug( "Window created successfully\n" );

    int window_width, window_height;
    SDL_Vulkan_GetDrawableSize( window, &window_width, &window_height );

    width = ( u32 )window_width;
    height = ( u32 )window_height;

    // Assing this so it can be accessed from outside.
    platform_handle = window;

    is_running = true;

    //// https://bobobobo.wordpress.com/2008/02/03/getting-the-hwnd-and-hinstance-of-the-console-window/
    //// Retrieve console window handle to retrieve instance handle
    //char t[ 512 ];
    //GetConsoleTitleA( t, 512 );
    //HWND hwndConsole = FindWindowA( NULL, t );
    //// NOTE(marco): this wasn't working for me. The article above says GetModuleHandle is a valid alternative
    //// LONG_PTR instance_handle = ( HINSTANCE )GetWindowLongPtrA( hwndConsole, GWLP_HINSTANCE );
    //HINSTANCE instance_handle = GetModuleHandle( 0 );
    //iassert( instance_handle );

    //// Register window class
    //WNDCLASS wc = { };
    //wc.lpfnWndProc = window_procedure;
    //wc.hInstance = instance_handle;
    //wc.lpszClassName = "IdraWindow";
    //if ( !RegisterClassA( &wc ) ) {
    //    MessageBoxA( NULL, "Window Registration Failed!", "Error!",
    //                 MB_ICONEXCLAMATION | MB_OK );
    //    return;
    //}
    //// Create window
    //// NOTE: window procedure needs to handle the WM_NCCREATE message,
    //// normally handled in DefWindowProc.
    //// https://stackoverflow.com/questions/8550679/createwindowex-function-fails-but-getlasterror-returns-error-success
    //// Last error will return "E_SUCCESS" whilst failing.
    //HWND window_handle = CreateWindowExA(
    //    WS_EX_CLIENTEDGE,
    //    wc.lpszClassName,
    //    name.data,
    //    WS_OVERLAPPEDWINDOW,
    //    CW_USEDEFAULT, CW_USEDEFAULT, width, height,
    //    NULL, NULL, instance_handle, this );

    //if ( window_handle == NULL ) {
    //    MessageBoxA( NULL, "Window Creation Failed!", "Error!",
    //                 MB_ICONEXCLAMATION | MB_OK );
    //    return;
    //}

    //ShowWindow( window_handle, SW_SHOWNORMAL );
    //UpdateWindow( window_handle );

    //this->width = width;
    //this->height = height;

    //platform_handle = window_handle;
}

void Window::shutdown() {

    SDL_DestroyWindow( window );
    SDL_Quit();

}

void Window::handle_os_messages() {

    SDL_Event event;
    while ( SDL_PollEvent( &event ) ) {

        ImGui_ImplSDL2_ProcessEvent( &event );

        switch ( event.type ) {
            case SDL_QUIT:
            {
                is_running = false;
                //requested_exit = true;
                //goto propagate_event;
                break;
            }

            case SDL_KEYDOWN:
            {
                i32 key = event.key.keysym.scancode;
                //if ( key >= 0 && key < ( i32 )num_keys )
                if ( input ) {
                    input->keyboard_current.keys[ key ] = 1;
                }
                break;
            }

            case SDL_KEYUP:
            {
                i32 key = event.key.keysym.scancode;
                //if ( key >= 0 && key < ( i32 )num_keys )
                if ( input ) {
                    input->keyboard_current.keys[ key ] = 0;
                }
                break;
            }

            case SDL_CONTROLLERDEVICEADDED:
            {
                ilog_debug( "Gamepad Added\n" );

                if ( input ) {
                    int32_t index = event.cdevice.which;
                    input->init_gamepad( index );
                }

                break;
            }

            case SDL_CONTROLLERDEVICEREMOVED:
            {
                ilog_debug( "Gamepad Removed\n" );
                int32_t instance_id = event.jdevice.which;

                if ( input ) {
                    input->shutdown_gamepad( instance_id );
                }

                break;
            }

            case SDL_CONTROLLERAXISMOTION:
            {
#if defined (INPUT_DEBUG_OUTPUT)
                ilog_debug( "Axis %u - %d\n", event.jaxis.axis, event.jaxis.value );
#endif // INPUT_DEBUG_OUTPUT

                if ( input ) {
                    input->set_gamepad_axis_value( event.caxis.which, (GamepadAxis)event.caxis.axis, event.caxis.value / 32768.0f );
                }

                break;
            }

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            {
#if defined (INPUT_DEBUG_OUTPUT)
                ilog_debug( "Button %u\n", event.cbutton.button );
#endif // INPUT_DEBUG_OUTPUT

                if ( input ) {
                    input->set_gamepad_button( event.cbutton.which, (GamepadButtons)event.cbutton.button, event.cbutton.state == SDL_PRESSED ? 1 : 0 );
                }

                break;
            }

            // Handle subevent
            case SDL_WINDOWEVENT:
            {
                switch ( event.window.event ) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        // Resize only if even numbers are used?
                        // NOTE: This goes in an infinite loop when maximising a window that has an odd width/height.
                        /*if ( ( event.window.data1 % 2 == 1 ) || ( event.window.data2 % 2 == 1 ) ) {
                            u32 new_width = ( u32 )( event.window.data1 % 2 == 0 ? event.window.data1 : event.window.data1 - 1 );
                            u32 new_height = ( u32 )( event.window.data2 % 2 == 0 ? event.window.data2 : event.window.data2 - 1 );

                            if ( new_width != width || new_height != height ) {
                                SDL_SetWindowSize( window, new_width, new_height );

                                ilog_debug( "Forcing resize to a multiple of 2, %ux%u from %ux%u\n", new_width, new_height, event.window.data1, event.window.data2 );
                            }
                        }*/
                        //else 
                        {
                            u32 new_width = ( u32 )( event.window.data1 );
                            u32 new_height = ( u32 )( event.window.data2 );

                            // Update only if needed.
                            if ( new_width != width || new_height != height ) {
                                resized = true;
                                width = new_width;
                                height = new_height;

                                ilog_debug( "Resizing to %u, %u\n", width, height );
                            }
                        }

                        break;
                    }

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    {
                        ilog_debug( "Focus Gained\n" );
                        if ( input ) {
                            input->has_focus = true;
                        }
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                    {
                        ilog_debug( "Focus Lost\n" );
                        if ( input ) {
                            input->has_focus = false;
                        }
                        break;
                    }
                    case SDL_WINDOWEVENT_MAXIMIZED:
                    {
                        ilog_debug( "Maximized\n" );
                        minimized = false;
                        break;
                    }
                    case SDL_WINDOWEVENT_MINIMIZED:
                    {
                        ilog_debug( "Minimized\n" );
                        minimized = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_RESTORED:
                    {
                        ilog_debug( "Restored\n" );
                        minimized = false;
                        break;
                    }
                    case SDL_WINDOWEVENT_TAKE_FOCUS:
                    {
                        ilog_debug( "Take Focus\n" );
                        break;
                    }
                    case SDL_WINDOWEVENT_EXPOSED:
                    {
                        ilog_debug( "Exposed\n" );
                        break;
                    }

                    case SDL_WINDOWEVENT_CLOSE:
                    {
                        //requested_exit = true;
                        is_running = false;
                        ilog_debug( "Window close event received.\n" );
                        break;
                    }

                    default:
                    {
                        //display_refresh = sdl_get_monitor_refresh();
                        break;
                    }
                }
                //goto propagate_event;
                break;
            }
        }
        // Maverick: 
    //propagate_event:
    //    // Callbacks
    //    for ( u32 i = 0; i < os_messages_callbacks.size; ++i ) {
    //        OsMessagesCallback callback = os_messages_callbacks[ i ];
    //        callback( &event, os_messages_callbacks_data[ i ] );
    //    }
    }
}


void Window::set_fullscreen( bool value ) {
    if ( value )
        SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN_DESKTOP );
    else {
        SDL_SetWindowFullscreen( window, 0 );
    }
}

void Window::center_mouse( bool dragging ) {
    if ( dragging ) {
        SDL_WarpMouseInWindow( window, idra::roundu32( width / 2.f ), idra::roundu32( height / 2.f ) );
        SDL_SetWindowGrab( window, SDL_TRUE );
        SDL_SetRelativeMouseMode( SDL_TRUE );
    } else {
        SDL_SetWindowGrab( window, SDL_FALSE );
        SDL_SetRelativeMouseMode( SDL_FALSE );
    }
}

//
//// https://stackoverflow.com/questions/5681284/how-do-i-distinguish-between-left-and-right-keys-ctrl-and-alt
//WPARAM map_left_right_keys( WPARAM vk, LPARAM lParam ) {
//    WPARAM new_vk = vk;
//    UINT scancode = ( lParam & 0x00ff0000 ) >> 16;
//    int extended = ( lParam & 0x01000000 ) != 0;
//
//    switch ( vk ) {
//        case VK_SHIFT:
//            new_vk = MapVirtualKey( scancode, MAPVK_VSC_TO_VK_EX );
//            break;
//        case VK_CONTROL:
//            new_vk = extended ? VK_RCONTROL : VK_LCONTROL;
//            break;
//        case VK_MENU:   // Menu is alt!
//            new_vk = extended ? VK_RMENU : VK_LMENU;
//            break;
//        default:
//            // not a key we map from generic to left/right specialized
//            //  just return it.
//            new_vk = vk;
//            break;
//    }
//
//    return new_vk;
//}

//// Window procedure used to process each message
//LRESULT CALLBACK window_procedure( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
//
//    // First read imgui messages, see if they capture input
//    LRESULT imgui_result = ImGui_ImplWin32_WndProcHandler( hwnd, msg, wParam, lParam );
//    if ( imgui_result ) {
//        return imgui_result;
//    }
//    // Retrieve user data storing window pointer
//    Window* window = ( Window* )GetWindowLongPtrA( hwnd, GWLP_USERDATA );
//
//    switch ( msg ) {
//        case WM_CREATE:
//        {
//            // Check if window is null, then store it in user-data
//            if ( window == nullptr ) {
//                CREATESTRUCT* createStruct = ( CREATESTRUCT* )( lParam );
//                window = static_cast< Window* >( createStruct->lpCreateParams );
//                SetWindowLongPtrA( hwnd, GWLP_USERDATA, ( LONG_PTR )window );
//                // Start window
//                window->is_running = true;
//            }
//            break;
//        }
//        case WM_CLOSE:
//        {
//            DestroyWindow( hwnd );
//            break;
//        }
//        case WM_DESTROY:
//        {
//            window->is_running = false;
//
//            PostQuitMessage( 0 );
//            break;
//        }
//
//        case WM_SIZE:
//        {
//            u32 new_width = LOWORD( lParam );
//            u32 new_height = HIWORD( lParam );
//
//            // Update only if needed.
//            if ( new_width != window->width || new_height != window->height ) {
//                window->resized = true;
//                window->width = new_width;
//                window->height = new_height;
//
//                ilog_debug( "Resizing to %u, %u\n", new_width, new_height );
//            }
//            break;
//        }
//        case WM_KEYUP:
//        case WM_SYSKEYUP:
//        {
//            // Translate left/right keys if needed
//            wParam = map_left_right_keys( wParam, lParam );
//            Keys key = key_translate( wParam );
//
//            if ( window->input ) {
//                window->input->keyboard_current.keys[ key ] = 0;
//            }
//
//            ilog_debug( "%s v%u k%u %s\n", "Released", wParam, key, key_names()[ key ] );
//            break;
//        }
//        case WM_KEYDOWN:
//        case WM_SYSKEYDOWN:
//        {
//            // Translate left/right keys if needed
//            wParam = map_left_right_keys( wParam, lParam );
//            Keys key = key_translate( wParam );
//
//            if ( window->input ) {
//                window->input->keyboard_current.keys[ key ] = 1;
//            }
//
//            ilog_debug( "%s v%u k%u %s\n", "Pressed", wParam, key, key_names()[ key ] );
//            break;
//        }
//        case WM_INPUT:
//        {
//            // Not needed for now
//#if 0
//                // Using this as reference: https://blog.molecular-matters.com/2011/09/05/properly-handling-keyboard-input/
//            HRAWINPUT handle = ( HRAWINPUT )lParam;
//            UINT size;
//            GetRawInputData( handle, RID_INPUT, NULL, &size, sizeof( RAWINPUTHEADER ) );
//            u8 input_buffer[ 2048 ];
//            GetRawInputData( handle, RID_INPUT, input_buffer, &size, sizeof( RAWINPUTHEADER ) );
//            RAWINPUT* raw_input = ( RAWINPUT* )input_buffer;
//            if ( raw_input->header.dwType == RIM_TYPEKEYBOARD ) {
//                const RAWKEYBOARD& raw_keyboard = raw_input->data.keyboard;
//                UINT virtual_key = raw_keyboard.VKey;
//                UINT scan_code = raw_keyboard.MakeCode;
//                UINT flags = raw_keyboard.Flags;
//
//                bool key_down = ( flags & 0x1 ) == RI_KEY_MAKE;
//
//                Keys key = key_translate( virtual_key );
//                ilog_debug( "%s v%u k%u s%u %s\n", key_down ? "Pressed" : "Released", virtual_key, key, scan_code, key_names()[ key ] );
//            }
//#endif //
//            break;
//        }
//
//        {
//            break;
//        }
//        case WM_MOUSEMOVE:
//        {
//            const i16 mouse_x = ( i16 )lParam;
//            const i16 mouse_y = ( i16 )( lParam >> 16 );
//
//            InputSystem* input = window->input;
//            if ( !window->input->mouse_first_event ) {
//                input->mouse_delta[ 0 ] = mouse_x - input->mouse_current.position[0];
//                input->mouse_delta[ 1 ] = mouse_y - input->mouse_current.position[1];
//            }
//            input->mouse_current.position[ 0 ] = mouse_x;
//            input->mouse_current.position[ 1 ] = mouse_y;
//            input->mouse_first_event = true;
//            //ilog_debug( "Mouse position %d, %d\n", mouse_x, mouse_y );
//
//            break;
//        }
//        case WM_LBUTTONDOWN:
//        {
//            InputSystem* input = window->input;
//            input->mouse_current.buttons[ MOUSE_BUTTONS_LEFT ] = 1;
//            break;
//        }
//        case WM_RBUTTONDOWN:
//        {
//            InputSystem* input = window->input;
//            input->mouse_current.buttons[ MOUSE_BUTTONS_RIGHT ] = 1;
//            break;
//        }
//        case WM_MBUTTONDOWN:
//        {
//            InputSystem* input = window->input;
//            input->mouse_current.buttons[ MOUSE_BUTTONS_MIDDLE ] = 1;
//            break;
//        }
//        case WM_LBUTTONUP:
//        {
//            InputSystem* input = window->input;
//            input->mouse_current.buttons[ MOUSE_BUTTONS_LEFT ] = 0;
//            break;
//        }
//        case WM_RBUTTONUP:
//        {
//            InputSystem* input = window->input;
//            input->mouse_current.buttons[ MOUSE_BUTTONS_RIGHT ] = 0;
//            break;
//        }
//        case WM_MBUTTONUP:
//        {
//            InputSystem* input = window->input;
//            input->mouse_current.buttons[ MOUSE_BUTTONS_MIDDLE ] = 0;
//            break;
//        }
//        default:
//            return DefWindowProcA( hwnd, msg, wParam, lParam );
//    }
//
//    return 0;
//}

} // namespace idra
