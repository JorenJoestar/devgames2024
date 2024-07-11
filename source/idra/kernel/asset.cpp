#include "kernel/asset.hpp"
#include "kernel/assert.hpp"

namespace idra {

static AssetManager s_asset_manager;
static constexpr u32 k_max_path = 64;

AssetManager* AssetManager::init_system() {

    for ( u32 i = 0; i < 32; ++i ) {
        s_asset_manager.loaders[ i ] = nullptr;
    }

    s_asset_manager.path_string_pool.init( g_memory->get_resident_allocator(), 128, k_max_path );

    return &s_asset_manager;
}

void AssetManager::shutdown_system( AssetManager* instance ) {

    iassert( instance == &s_asset_manager );
    // TODO: check loaders 
    for ( u32 i = 0; i < 32; ++i ) {
        if ( s_asset_manager.loaders[ i ] ) {
            s_asset_manager.loaders[ i ]->shutdown();
        }
    }

    s_asset_manager.path_string_pool.shutdown();
}

void AssetManager::set_loader( u32 index, AssetLoaderBase* loader ) {

    iassert( loaders[ index ] == nullptr );

    if ( loaders[ index ] == nullptr ) {
        loaders[ index ] = loader;
    }
}

AssetPath AssetManager::allocate_path( StringView path ) {

    AssetPath asset_path;

    // TODO: max 64 characters for now.
    iassert( path.size < k_max_path );
    // Allocate a string
    asset_path.pool_index = path_string_pool.obtain_resource();
    void* string_data = path_string_pool.access_resource( asset_path.pool_index );
    memcpy( string_data, path.data, path.size );
    // Update the path
    asset_path.path.data = ( cstring )string_data;
    asset_path.path.size = path.size;

    return asset_path;
}

void AssetManager::free_path( AssetPath& path ) {

    path_string_pool.release_resource( path.pool_index );
}


} // namespace idra