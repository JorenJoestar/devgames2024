/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "imgui/widgets.hpp"

#include "kernel/hash_map.hpp"
#include "kernel/file.hpp"
#include "kernel/string.hpp"
#include "kernel/allocator.hpp"
#include "kernel/camera.hpp"
#include "kernel/numerics.hpp"
#include "kernel/input.hpp"

#include "gpu/gpu_device.hpp"

#include "application/game_camera.hpp"

#include "tools/shader_compiler/shader_compiler.hpp"

#include <filesystem>

namespace ImGui {

// Log Widget /////////////////////////////////////////////////////////////

void ApplicationLog::init() {
    auto_scroll = true;
    clear();
}

void ApplicationLog::shutdown() {
}

void ApplicationLog::clear() {
    buf.clear();
    line_offsets.clear();
    line_offsets.push_back( 0 );
}

void ApplicationLog::add_log( const char* fmt, ... ) {

    int old_size = buf.size();
    va_list args;
    va_start( args, fmt );
    buf.appendfv( fmt, args );
    va_end( args );
    for ( int new_size = buf.size(); old_size < new_size; old_size++ ) {
        if ( buf[ old_size ] == '\n' ) {
            line_offsets.push_back( old_size + 1 );
        }
    }
}

void ApplicationLog::draw( const char* title, bool* p_open ) {

    if ( !ImGui::Begin( title, p_open ) ) {
        ImGui::End();
        return;
    }

    // Options menu
    if ( ImGui::BeginPopup( "Options" ) ) {
        ImGui::Checkbox( "Auto-scroll", &auto_scroll );
        ImGui::EndPopup();
    }

    // Main window
    if ( ImGui::Button( "Options" ) )
        ImGui::OpenPopup( "Options" );

    ImGui::SameLine();
    bool clear_button_enabled = ImGui::Button( "Clear" );
    ImGui::SameLine();
    bool copy = ImGui::Button( "Copy" );
    ImGui::SameLine();
    filter.Draw( "Filter", -100.0f );

    ImGui::Separator();
    ImGui::BeginChild( "scrolling", ImVec2( 0, 0 ), false, ImGuiWindowFlags_HorizontalScrollbar );

    if ( clear_button_enabled ) {
        clear();
    }

    if ( copy ) {
        ImGui::LogToClipboard();
    }

    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
    const char* buf_start = buf.begin();
    const char* buf_end = buf.end();
    if ( filter.IsActive() ) {
        // In this example we don't use the clipper when Filter is enabled.
        // This is because we don't have a random access on the result on our filter.
        // A real application processing logs with ten of thousands of entries may want to store the result of search/filter.
        // especially if the filtering function is not trivial (e.g. reg-exp).
        for ( int line_no = 0; line_no < line_offsets.Size; line_no++ ) {
            const char* line_start = buf_start + line_offsets[ line_no ];
            const char* line_end = ( line_no + 1 < line_offsets.Size ) ? ( buf_start + line_offsets[ line_no + 1 ] - 1 ) : buf_end;
            if ( filter.PassFilter( line_start, line_end ) ) {
                ImGui::TextUnformatted( line_start, line_end );
            }
        }
    } else {
        // The simplest and easy way to display the entire buffer:
        //   ImGui::TextUnformatted(buf_begin, buf_end);
        // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward to skip non-visible lines.
        // Here we instead demonstrate using the clipper to only process lines that are within the visible area.
        // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them on your side is recommended.
        // Using ImGuiListClipper requires A) random access into your data, and B) items all being the  same height,
        // both of which we can handle since we an array pointing to the beginning of each line of text.
        // When using the filter (in the block of code above) we don't have random access into the data to display anymore, which is why we don't use the clipper.
        // Storing or skimming through the search result would make it possible (and would be recommended if you want to search through tens of thousands of entries)
        ImGuiListClipper clipper;
        clipper.Begin( line_offsets.Size );
        while ( clipper.Step() ) {
            for ( int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++ ) {
                const char* line_start = buf_start + line_offsets[ line_no ];
                const char* line_end = ( line_no + 1 < line_offsets.Size ) ? ( buf_start + line_offsets[ line_no + 1 ] - 1 ) : buf_end;
                ImGui::TextUnformatted( line_start, line_end );
            }
        }
        clipper.End();
    }
    ImGui::PopStyleVar();

    if ( auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() )
        ImGui::SetScrollHereY( 1.0f );

    ImGui::EndChild();
    ImGui::End();
}

static ApplicationLog s_imgui_log;

static void imgui_log_widget_print( cstring text ) {
    s_imgui_log.add_log( "%s", text );
}

void ApplicationLogInit() {
    idra::g_log->add_callback( &imgui_log_widget_print );

    idra::shader_compiler_add_log_callback( &imgui_log_widget_print );
}

void ApplicationLogShutdown() {
    idra::g_log->remove_callback( &imgui_log_widget_print );

    idra::shader_compiler_remove_log_callback( &imgui_log_widget_print );
}

void ApplicationLogDraw() {
    s_imgui_log.draw( "Log", &s_imgui_log.open_window );
}


// Sparkline //////////////////////////////////////////////////////////////
// Plot with ringbuffer

// https://github.com/leiradel/ImGuiAl
template<typename T, size_t L>
class Sparkline {
public:
    Sparkline() {
        setLimits( 0, 1 );
        clear();
    }

    void setLimits( T const min, T const max ) {
        _min = static_cast< float >( min );
        _max = static_cast< float >( max );
    }

    void add( T const value ) {
        _offset = ( _offset + 1 ) % L;
        _values[ _offset ] = value;
    }

    void clear() {
        memset( _values, 0, L * sizeof( T ) );
        _offset = L - 1;
    }

    void draw( char const* const label = "", ImVec2 const size = ImVec2() ) const {
        char overlay[ 32 ];
        print( overlay, sizeof( overlay ), _values[ _offset ] );

        ImGui::PlotLines( label, getValue, const_cast< Sparkline* >( this ), L, 0, overlay, _min, _max, size );
    }

protected:
    float _min, _max;
    T _values[ L ];
    size_t _offset;

    static float getValue( void* const data, int const idx ) {
        Sparkline const* const self = static_cast< Sparkline* >( data );
        size_t const index = ( idx + self->_offset + 1 ) % L;
        return static_cast< float >( self->_values[ index ] );
    }

    static void print( char* const buffer, size_t const bufferLen, int const value ) {
        snprintf( buffer, bufferLen, "%d", value );
    }

    static void print( char* const buffer, size_t const bufferLen, double const value ) {
        snprintf( buffer, bufferLen, "%f", value );
    }
};

static Sparkline<f32, 100> s_fps_line;

// FPS widget /////////////////////////////////////////////////////////////

void FPSInit( f32 max_value ) {
    s_fps_line.clear();
    s_fps_line.setLimits( 0.0f, max_value );
}

void FPSShutdown() {
}

void FPSAdd( f32 delta_time ) {
    s_fps_line.add( delta_time );
}

void FPSDraw( f32 width, f32 height ) {
    s_fps_line.draw( "Ms", { width, height } );
}


// File Dialog //////////////////////////////////////////////////////////////////

// File dialog

#define MAX_PATH 256

struct FileDialog {

    void                        init();
    void                        shutdown();

    bool                        open( const char* button_name,
                                      const char* path,
                                      const char* extension );

    const char*                 get_filename() const;

    idra::FlatHashMap<u64, bool> dialog_open_map;
    idra::Directory             directory;
    char                        filename[ MAX_PATH ];
    char                        last_path[ MAX_PATH ];
    char                        last_extension[ 16 ];

    bool                        scan_folder = true;

    idra::StringArray           files;
    idra::StringArray           directories;
};

struct DirectoryDialog {

    void                        init();
    void                        shutdown();

    bool                        open( const char* button_name,
                                      const char* path );

    const char*                 get_path() const;


    idra::FlatHashMap<u64, bool> dialog_open_map;
    idra::Directory             directory;
    char                        last_path[ MAX_PATH ];

    bool                        scan_folder = true;

    idra::StringArray           files;
    idra::StringArray           directories;
};


static FileDialog               s_file_dialog;
static DirectoryDialog          s_directory_dialog;


void FileDialog::init() {

    idra::Allocator* allocator = idra::g_memory->get_resident_allocator();
    // File dialog init
    files.init( 10000, allocator );
    directories.init( 10000, allocator );

    dialog_open_map.init( allocator, 8 );

    filename[ 0 ] = 0;
    last_extension[ 0 ] = 0;
    last_path[ 0 ] = 0;
}

void FileDialog::shutdown() {
    files.shutdown();
    directories.shutdown();

    dialog_open_map.shutdown();
}

bool FileDialog::open( const char* button_name, const char* path,
                       const char* extension ) {
    const u64 hashed_name = idra::hash_calculate( button_name );
    //bool opened = string_hash_get( file_dialog_open_map, button_name );
    bool opened = dialog_open_map.get( hashed_name );
    if ( ImGui::Button( button_name ) ) {
        opened = true;
    }

    bool selected = false;

    if ( opened && ImGui::Begin( "Idra File Dialog", &opened, ImGuiWindowFlags_AlwaysAutoResize ) ) {

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 20, 20 ) );
        ImGui::Text( directory.path );
        ImGui::PopStyleVar();

        ImGui::Separator();

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 20, 4 ) );

        if ( strcmp( path, last_path ) != 0 ) {
            strcpy( last_path, path );

            idra::fs_open_directory( path, &directory );

            scan_folder = true;
        }

        if ( strcmp( extension, last_extension ) != 0 ) {
            strcpy( last_extension, extension );

            scan_folder = true;
        }

        // Search files
        if ( scan_folder ) {
            scan_folder = false;

            idra::fs_find_files_in_path( extension, directory.path, files, directories );
        }

        for ( u32 i = 0; i < directories.get_string_count(); ++i ) {

            const char* directory_name = directories.get_string( i );
            if ( ImGui::Selectable( directory_name, selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {

                if ( strcmp( directory_name, ".." ) == 0 ) {
                    idra::fs_parent_directory( &directory );
                } else {
                    idra::fs_sub_directory( &directory, directory_name );
                }

                scan_folder = true;
            }
        }

        for ( u32 i = 0; i < files.get_string_count(); ++i ) {

            const char* file_name = files.get_string( i );
            if ( ImGui::Selectable( file_name, selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {

                strcpy( filename, directory.path );
                filename[ strlen( filename ) - 1 ] = 0;
                strcat( filename, file_name );

                selected = true;
                opened = false;
            }
        }

        ImGui::PopStyleVar();

        ImGui::End();
    }

    // Update opened map
    dialog_open_map.insert( hashed_name, opened );

    return selected;
}

const char* FileDialog::get_filename() const {
    return filename;
}

void FileDialogInit() {
    s_file_dialog.init();
}

void FileDialogShutdown() {
    s_file_dialog.shutdown();
}

bool FileDialogOpen( const char* button_name, const char* path, const char* extension ) {
    return s_file_dialog.open( button_name, path, extension );
}

const char* FileDialogGetFilename() {
    return s_file_dialog.get_filename();
}


// Directory Dialog ///////////////////////////////////////////////////////

void DirectoryDialog::init() {
    idra::Allocator* allocator = idra::g_memory->get_resident_allocator();
    // File dialog init
    files.init( 10000, allocator );
    directories.init( 10000, allocator );

    dialog_open_map.init( allocator, 8 );

    last_path[ 0 ] = 0;
}

void DirectoryDialog::shutdown() {

    files.shutdown();
    directories.shutdown();

    dialog_open_map.shutdown();
}

bool DirectoryDialog::open( const char* button_name, const char* path ) {
    const u64 hashed_name = idra::hash_calculate( button_name );
    //bool opened = string_hash_get( file_dialog_open_map, button_name );
    bool opened = dialog_open_map.get( hashed_name );
    if ( ImGui::Button( button_name ) ) {
        opened = true;
    }

    bool selected = false;

    if ( opened && ImGui::Begin( "Idra Path Dialog", &opened, ImGuiWindowFlags_AlwaysAutoResize ) ) {

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 20, 20 ) );
        ImGui::Text( directory.path );
        ImGui::PopStyleVar();

        ImGui::Separator();

        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 20, 4 ) );

        if ( strcmp( path, last_path ) != 0 ) {
            strcpy( last_path, path );

            idra::fs_open_directory( path, &directory );

            scan_folder = true;
        }

        // Search files
        if ( scan_folder ) {
            scan_folder = false;

            idra::fs_find_files_in_path( ".", directory.path, files, directories );
        }

        for ( u32 i = 0; i < directories.get_string_count(); ++i ) {

            const char* directory_name = directories.get_string( i );
            // > 1 to skip current path
            if ( ( strlen( directory_name ) > 1 ) && ImGui::Selectable( directory_name, selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {

                if ( strcmp( directory_name, ".." ) == 0 ) {
                    idra::fs_parent_directory( &directory );
                } else {
                    idra::fs_sub_directory( &directory, directory_name );
                }

                scan_folder = true;
            }
        }

        if ( ImGui::Button( "Choose Current Folder" ) ) {
            strcpy( last_path, directory.path );
            // Remove the asterisk
            last_path[ strlen( last_path ) - 1 ] = 0;

            selected = true;
            opened = false;
        }
        ImGui::SameLine();
        if ( ImGui::Button( "Cancel" ) ) {
            opened = false;
        }

        ImGui::PopStyleVar();

        ImGui::End();
    }

    // Update opened map
    dialog_open_map.insert( hashed_name, opened );

    return selected;
}

const char* DirectoryDialog::get_path() const {
    return last_path;
}

void DirectoryDialogInit() {
    s_directory_dialog.init();
}

void DirectoryDialogShutdown() {
    s_directory_dialog.shutdown();
}

bool DirectoryDialogOpen( const char* button_name, const char* path ) {
    return s_directory_dialog.open( button_name, path );
}

const char* DirectoryDialogGetPath() {
    return s_directory_dialog.get_path();
}

// Content Browser ////////////////////////////////////////////////////////

void ContentBrowserDraw() {
    static bool s_content_browser_open = true;
    if ( ImGui::Begin( "Content Browser" ), &s_content_browser_open ) {

        static float iconScaleSliderValue = 1.f;

        const float globalImguiScale = ImGui::GetIO().FontGlobalScale;
        const ImVec2 fileSize{ 92 * globalImguiScale * iconScaleSliderValue, 92 * globalImguiScale * iconScaleSliderValue };

        ImGui::BeginGroup();

        ImGui::BeginChild( "Content", ImVec2{ 0, -ImGui::GetFrameHeightWithSpacing() }, true );

        //if ( !_selectedDirectory.empty() )
        {

            int columns = idra::roundi32( ImGui::GetWindowContentRegionWidth() / fileSize.x - 1 );
            if ( columns <= 0 || columns > 64 ) {
                columns = 1;
            }

            ImGui::Columns( columns, nullptr, false );
            ImGui::Separator();

            int buttonID = 1;
            bool isFileSelected = false;

            for ( auto const& dir_entry : std::filesystem::directory_iterator( "../data/textures" ) ) {
                if ( !( dir_entry.is_regular_file() || dir_entry.is_directory() ) ) {
                    continue;
                }

                {
                    ImGui::BeginGroup();

                    if ( dir_entry.is_regular_file() ) {
                        ImGui::PushID( buttonID++ );

                        /*std::shared_ptr<jleTexture> iconTexture;
                        if ( dir_entry.path().extension() == ".scn" ) {
                            iconTexture = _sceneFileIcon;
                        } else if ( dir_entry.path().extension() == ".png" || dir_entry.path().extension() == ".jpg" ||
                                    dir_entry.path().extension() == ".tga" || dir_entry.path().extension() == ".bmp" ||
                                    dir_entry.path().extension() == ".psd" ) {
                            auto path = jlePath{ dir_entry.path().string(), false };
                            auto it = _referencedTextures.find( path );
                            if ( it != _referencedTextures.end() ) {
                                iconTexture = it->second;
                            } else {
                                iconTexture = gEngine->resources().loadResourceFromFile<jleTexture>(
                                    jlePath{ dir_entry.path().string(), false } );
                                _referencedTextures.insert( std::make_pair( path, iconTexture ) );
                            }
                        } else if ( dir_entry.path().extension() == ".json" ) {
                            iconTexture = _jsonFileIcon;
                        } else if ( dir_entry.path().extension() == ".lua" ) {
                            iconTexture = _luaFileIcon;
                        } else if ( dir_entry.path().extension() == ".glsl" ) {
                            iconTexture = _shaderFileIcon;
                        } else if ( dir_entry.path().extension() == ".mat" ) {
                            iconTexture = _materialFileIcon;
                        } else if ( dir_entry.path().extension() == ".jobj" ) {
                            iconTexture = _objTemplateFileIcon;
                        } else if ( dir_entry.path().extension() == ".obj" || dir_entry.path().extension() == ".fbx" ) {
                            iconTexture = _obj3dFileIcon;
                        } else {
                            iconTexture = _fileIcon;
                        }*/

                        //if ( ImGui::ImageButton( &bayer_4x4_texture->texture, fileSize ) ) {
                        //    //_fileSelected = dir_entry;
                        //    isFileSelected = true;
                        //}
                        ImGui::PopID();
                    } else if ( dir_entry.is_directory() ) {
                        ImGui::PushID( buttonID++ );
                        //if ( ImGui::ImageButton( &bayer_4x4_texture->texture, fileSize ) ) {
                        //    //_selectedDirectory = dir_entry.path();
                        //}
                        ImGui::PopID();
                    }

                    ImGui::Text( "%s", dir_entry.path().filename().string().c_str() );

                    ImGui::Dummy( ImVec2( 0.0f, 4.0f * globalImguiScale ) );

                    ImGui::EndGroup();
                }

                ImGui::NextColumn();
            }

            /*if ( isFileSelected ) {
                ImGui::OpenPopup( "selected_file_popup" );
            }

            if ( !_fileSelected.empty() && ImGui::BeginPopup( "selected_file_popup" ) ) {

                selectedFilePopup( _fileSelected );
                ImGui::EndPopup();
            }*/

            ImGui::Columns( 1 );
            ImGui::Separator();
        }

        ImGui::EndChild();

        ImGui::PushItemWidth( 100 * globalImguiScale );
        ImGui::SliderFloat( "Icon Scale", &iconScaleSliderValue, 0.25f, 2.f );

        ImGui::EndGroup();
    }
    ImGui::End();
}

// Content Hierarchy //////////////////////////////////////////////////////

std::pair<bool, uint32_t> directoryTreeViewRecursive( const std::filesystem::path& path,
                                                      uint32_t* count,
                                                      int* selection_mask ) {
    ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

    bool any_node_clicked = false;
    uint32_t node_clicked = 0;

    for ( const auto& entry : std::filesystem::directory_iterator( path ) ) {
        ImGuiTreeNodeFlags node_flags = base_flags;
        /*const bool is_selected = ( *selection_mask & BIT( *count ) ) != 0;
        if ( is_selected )
            node_flags |= ImGuiTreeNodeFlags_Selected;*/

        std::string name = entry.path().string();

        auto lastSlash = name.find_last_of( "/\\" );
        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        name = name.substr( lastSlash, name.size() - lastSlash );

        bool entryIsFile = !std::filesystem::is_directory( entry.path() );
        if ( entryIsFile )
            node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        bool node_open = ImGui::TreeNodeEx( ( void* )( intptr_t )( *count ), node_flags, "%s", name.c_str() );

        if ( ImGui::IsItemClicked() ) {
            node_clicked = *count;
            any_node_clicked = true;
            /*if ( !entry.is_directory() ) {
                _selectedDirectory = entry.path().parent_path();
            } else if ( entry.is_directory() ) {
                _selectedDirectory = entry.path();
            }*/
        }

        ( *count )--;

        if ( !entryIsFile ) {
            if ( node_open ) {

                auto clickState = directoryTreeViewRecursive( entry.path(), count, selection_mask );

                if ( !any_node_clicked ) {
                    any_node_clicked = clickState.first;
                    node_clicked = clickState.second;
                }

                ImGui::TreePop();
            } else {
                for ( const auto& e : std::filesystem::recursive_directory_iterator( entry.path() ) )
                    ( *count )--;
            }
        }
    }

    return { any_node_clicked, node_clicked };
}

void ContentHierarchyDraw() {
    auto content_hierarchy = []( std::string directoryPath, const std::string& folderName ) {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );

        ImGui::Begin( "Content Hierarchy" );

        if ( ImGui::CollapsingHeader( folderName.c_str() ) ) {
            uint32_t count = 0;
            for ( const auto& entry : std::filesystem::recursive_directory_iterator( directoryPath ) )
                count++;

            static int selection_mask = 0;

            directoryTreeViewRecursive( directoryPath, &count, &selection_mask );

            //if ( clickState.first ) {
            //    // Update selection state
            //    // (process outside of tree loop to avoid visual inconsistencies
            //    // during the clicking frame)
            //    if ( ImGui::GetIO().KeyCtrl )
            //        selection_mask ^= BIT( clickState.second ); // CTRL+click to toggle
            //    else                                          // if (!(selection_mask & (1 << clickState.second))) //
            //                                                  // Depending on selection behavior you want, may want to
            //        // preserve selection when clicking on item that is part of the
            //        // selection
            //        selection_mask = BIT( clickState.second ); // Click to single-select
            //}
        }
        ImGui::End();

        ImGui::PopStyleVar();
        };

    content_hierarchy( "..//data", "Asset Folder" );
}

// ImGuiRenderView ////////////////////////////////////////////////////////

void ImGuiRenderView::init( idra::GameCamera* camera_, idra::Span<const idra::TextureHandle> textures_, idra::GpuDevice* gpu ) {
    camera = camera_;

    const sizet used_textures = idra::min<sizet>( textures_.size, k_max_textures );
    iassert( used_textures > 0 && used_textures <= k_max_textures );

    for ( u32 i = 0; i < used_textures; ++ i) {
        textures[ i ] = textures_[ i ];
    }

    num_textures = u32(used_textures);

    idra::Texture* texture_data = gpu->textures.get_cold( textures[ 0 ] );
    texture_width = texture_data->width;
    texture_height = texture_data->height;

    resized = false;
    focus = false;
}

void ImGuiRenderView::set_size( ImVec2 size ) {

    if ( size.x == texture_width && size.y == texture_height ) {
        return;
    }

    // View has been resized, update dimensions
    texture_width = ( f32 )idra::max( idra::roundi32( size.x ), 4 );
    texture_height = ( f32 )idra::max( idra::roundi32( size.y ), 4 );

    resized = true;
}

ImVec2 ImGuiRenderView::get_size() {
    return ImVec2{ texture_width, texture_height };
}

void ImGuiRenderView::check_resize( idra::GpuDevice* gpu, idra::InputSystem* input ) {

    if ( !resized ) {
        return;
    }

    // Wait for window resize to be completed
    // TODO: maybe have a timer to update at a lower refresh rate than the dragging ?
    if ( input->is_mouse_down( idra::MOUSE_BUTTONS_LEFT ) ) {
        return;
    }

    // No texture check
    idra::Texture* rt = gpu->textures.get_cold( textures[ 0 ] );
    if ( !rt ) {
        return;
    }

    // Resize all the dependant textures
    for ( u32 i = 0; i < num_textures; ++i ) {
        gpu->resize_texture( textures[ i ], idra::roundu32( texture_width ),
                             idra::roundu32( texture_height ) );
    }

    camera->camera.set_aspect_ratio( texture_width * 1.f / texture_height );
    camera->camera.set_viewport_size( texture_width, texture_height );
    camera->camera.update();
    ilog( "Resizing view to %f, %f\n", texture_width, texture_height );

    resized = false;
}

void ImGuiRenderView::draw( idra::StringView name ) {

    if ( ImGui::Begin( name.data ) ) {
        ImVec2 rt_size = ImGui::GetContentRegionAvail();

        set_size( rt_size );
        focus = ImGui::IsWindowFocused();

        // Show only the main texture
        ImGui::Image( textures[ 0 ], rt_size );
    }
    ImGui::End();
}

} // namespace ImGui
