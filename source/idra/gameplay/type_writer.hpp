/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string.hpp"

namespace idra {

struct Allocator;

// Useful links!
// https://github.com/rafaskb/typing-label/wiki/Tokens
// https://github.com/rafaskb/typing-label/wiki/Examples
// Usage: type_writer.StartWriting( "{COLOR=GREEN}Hello,{WAIT=1.0} world!{SPEED=1.0} This will be very slow. {WAIT=3.0} Ready ?" );
struct Parser {
    char* position;
}; // struct Parser

//
// Struct that write characters with a certain speed using commands.
//
struct TypeWriter {

    void                            init( Allocator* resident_allocator );
    void                            shutdown();

    void                            start_writing( StringView text );
    void                            update( f32 dt );

    void                            debug_ui();

    bool                            is_finished() const;
    bool                            has_finished_page() const;

    void                            next_page();
    void                            restart();

    void                            parse();


    struct Commands {
        // Commands are enclosed in {} parenthesis.
        enum CommandType {
            CommandType_Wait = 0,
            CommandType_Speed,
            CommandType_Write,
            CommandType_Page,
            CommandType_End,
        }; // enum CommandType
    }; // struct Commands

    static const u32                k_max_chars = 1024;

    Allocator*                      allocator = nullptr;
    cstring                         source_text = nullptr;
    char*                           output_text = nullptr;
    u32                             output_position = 0;
    u32                             output_length = 0;
    f32                             time = 0.f;
    f32                             command_time = 0.f;      // Time after executing the current command. Reset after each command.
    f32                             char_display_time = 0.1f;   // Seconds after a character is drawn.
    f32                             previous_char_display_time = 0.1f; // Used to restore speed after a page.
    f32                             s_standard_command_time = 0.1f;

    // Size metrics
    u32                             max_chars_per_line = 1;     // Max chars present in a line scanned through all text.
    u32                             max_lines_in_page = 1;

    Commands::CommandType           current_command = Commands::CommandType_Write;

    Parser                          parser;

}; // struct TypeWriter

} // namespace idra


