#include "gameplay/type_writer.hpp"

#include "kernel/memory.hpp"
#include "kernel/allocator.hpp"
#include "kernel/numerics.hpp"
#include "kernel/log.hpp"

#include <float.h>
#include <string>

#include "imgui/imgui.h"

namespace idra {


static void skip_character( Parser& p, char c ) {
    if ( *p.position != c ) {
        ilog_error( "Error! Found \n" );
    }
    ++p.position;
}

// Useful links!
// https://github.com/rafaskb/typing-label/wiki/Tokens
// https://github.com/rafaskb/typing-label/wiki/Examples
// Usage: type_writer.StartWriting( "{COLOR=GREEN}Hello,{WAIT=1.0} world!{SPEED=1.0} This will be very slow. {WAIT=3.0} Ready ?" );

void TypeWriter::init( Allocator* resident_allocator ) {

    allocator = resident_allocator;
    output_text = ( char* )ialloc( k_max_chars, allocator );
}

void TypeWriter::shutdown() {

    ifree( output_text, allocator );
}


void TypeWriter::start_writing( StringView text ) {

    source_text = text.data;

    // Scan for max number of chars
    // Cache max lines found in pages and use the max of all pages.
    max_lines_in_page = 1;
    max_chars_per_line = 1;
    u32 current_chars = 0, current_line_in_page = 1;

    char* text_to_scan = ( char* )source_text;
    while ( *text_to_scan != 0 ) {
        char c = *text_to_scan;

        // Skip commands form the calculation
        if ( c == '{' ) {
            // The command PAGE resets the content of a page, so need to save max chars
            if ( *( ++text_to_scan ) == 'P' ) {
                max_chars_per_line = idra::max( max_chars_per_line, current_chars );
                current_chars = 0;

                // Update line in pages
                max_lines_in_page = idra::max( max_lines_in_page, current_line_in_page );
                current_line_in_page = 0;
            }

            while ( *text_to_scan != '}' )
                ++text_to_scan;
        }

        ++current_chars;
        if ( ( c == '\n' ) || ( c == '\r' ) ) {
            max_chars_per_line = idra::max( max_chars_per_line, current_chars );
            current_chars = 0;
            ++current_line_in_page;
        }

        ++text_to_scan;
    }

    max_lines_in_page = idra::max( max_lines_in_page, current_line_in_page );
    max_chars_per_line = idra::max( max_chars_per_line, current_chars );

    restart();
}

void TypeWriter::restart() {
    time = 0.f;
    command_time = 0.f;
    char_display_time = s_standard_command_time;
    output_position = 0;

    output_text[ 0 ] = 0;
    output_length = ( u32 )strlen( source_text );
    parser.position = ( char* )source_text;

    parse();
}

void TypeWriter::update( f32 dt ) {

    time += dt;
    command_time += dt;

    if ( output_position >= k_max_chars ) {
        return;
    }

    if ( command_time > char_display_time && *parser.position ) {
        // Restore speed time if we are executing a wait. Parse will get next command.
        bool restore_char_time = current_command == Commands::CommandType_Wait;
        parse();

        if ( restore_char_time ) {
            char_display_time = previous_char_display_time;
        }

        if ( current_command == Commands::CommandType_Write ) {
            output_text[ output_position++ ] = *parser.position;
            output_text[ output_position ] = 0;

            parser.position++;
        }

        command_time = 0.f;
    }
}

void TypeWriter::debug_ui() {

    if ( ImGui::Begin( "Type writer" ) ) {

        ImGui::Text( "Welcome to the TypeWriter debug ui!\nSupported commands\n{PAGE},\n{SPEED=1.0}(characters per second)\n{WAIT=2.0}(seconds)" );

        ImGui::Text( "Source Text: %s", source_text );
        ImGui::Text( "Current Text: %s", output_text );
        if ( ImGui::Button( "Next page" ) ) {
            next_page();
        }
        if ( ImGui::Button( "Restart" ) ) {
            restart();
        }

        cstring status = "Writing";
        if ( has_finished_page() ) {
            status = "Finished Page";
        } else if ( is_finished() ) {
            status = "Finished.";
        }
        ImGui::Text( "Status: %s", status );

        static char text_buf[ TypeWriter::k_max_chars ];
        ImGui::InputText( "New text", text_buf, TypeWriter::k_max_chars, ImGuiInputTextFlags_None );
        if ( ImGui::Button( "Change text" ) ) {
            start_writing( text_buf );
        }
    }
    ImGui::End();
}

bool TypeWriter::is_finished() const {
    return parser.position == ( source_text + output_length );
}

bool TypeWriter::has_finished_page() const {
    return current_command == Commands::CommandType_Page;
}

void TypeWriter::next_page() {

    if ( current_command == Commands::CommandType_Page ) {
        current_command = Commands::CommandType_Write;
        // Reset writing output.
        // Parser already points to the new text.
        output_position = 0;
        output_text[ output_position ] = 0;
        // Restore writing using timer.
        char_display_time = previous_char_display_time;
    }
}

void TypeWriter::parse() {
    if ( *parser.position == '{' ) {
        parser.position++;

        switch ( *parser.position ) {
            case 'W':
            {
                skip_character( parser, 'W' );
                skip_character( parser, 'A' );
                skip_character( parser, 'I' );
                skip_character( parser, 'T' );
                skip_character( parser, '=' );

                char_display_time = strtof( parser.position, nullptr );

                while ( *parser.position != '}' ) {
                    ++parser.position;
                }

                ++parser.position;

                current_command = Commands::CommandType_Wait;

                break;
            }

            case 'C':
            {
                skip_character( parser, 'C' );
                skip_character( parser, 'O' );
                skip_character( parser, 'L' );
                skip_character( parser, 'O' );
                skip_character( parser, 'R' );
                skip_character( parser, '=' );

                while ( *parser.position != '}' ) {
                    ++parser.position;
                }

                ++parser.position;

                break;
            }

            case 'S':
            {
                skip_character( parser, 'S' );
                skip_character( parser, 'P' );
                skip_character( parser, 'E' );
                skip_character( parser, 'E' );
                skip_character( parser, 'D' );
                skip_character( parser, '=' );

                char_display_time = strtof( parser.position, nullptr );

                while ( *parser.position != '}' ) {
                    ++parser.position;
                }

                ++parser.position;

                current_command = Commands::CommandType_Speed;

                break;
            }

            case 'P':
            {
                skip_character( parser, 'P' );
                skip_character( parser, 'A' );
                skip_character( parser, 'G' );
                skip_character( parser, 'E' );

                while ( *parser.position != '}' ) {
                    ++parser.position;
                }

                ++parser.position;

                previous_char_display_time = char_display_time;
                // Block indefinitely output until next page is changed.
                char_display_time = FLT_MAX;

                current_command = Commands::CommandType_Page;
                break;
            }

            case 'E':
            {
                skip_character( parser, 'E' );
                skip_character( parser, 'N' );
                skip_character( parser, 'D' );

                while ( *parser.position != '}' ) {
                    ++parser.position;
                }

                ++parser.position;

                current_command = Commands::CommandType_End;
                break;
            }
        }
    } else {
        current_command = Commands::CommandType_Write;
    }
}

/*
To test this, you can do the following:
    // Init
    TypeWriter type_writer;
    type_writer.init( allocator );
    type_writer.start_writing( "{COLOR=GREEN}Hello,{WAIT=1.0} world!{SPEED=1.0} This will be very slow. {WAIT=3.0} Ready ?" );

    // Update and Render with ImGui
    ImGui::Begin( "Typewriter example" );
    ImGui::Text( "%s", type_writer.output_text );
    ImGui::End();
    type_writer.update( delta_time );

    // Goto next page if present.
    if ( input->is_key_pressed( return ) ) {
        type_writer.next_page();
    }

    // End
    type_writer.shutdown();
*/


} // namespace idra