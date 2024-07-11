/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/pool.hpp"
#include "kernel/hash_map.hpp"

namespace idra {

// Forward declarations
struct AssetManager;

//
//
namespace AssetCreationPhase {
    enum Enum : u8 {
        Startup,
        Reload,
        Count
    }; // enum Enum
} // namespace AssetCreationPhase

namespace AssetDestructionPhase {
    enum Enum : u8 {
        Shutdown,
        Reload,
        Count
    }; // enum Enum
} // namespace AssetDestructionPhase

//
//
struct AssetPath {
    StringView      path;
    u16             pool_index;
}; // struct AssetPath

//
//
struct Asset {

    u32             reference_count;

    u16             type;
    u16             pool_index;

    AssetPath       path;
}; // struct Asset


//
//
struct AssetLoaderBase {
    virtual void            init( Allocator* allocator, u32 size, AssetManager* asset_manager ) = 0;
    virtual void            shutdown() = 0;
}; // struct AssetLoaderBase

template <typename T>
struct AssetLoader : public AssetLoaderBase {

    void                    init( Allocator* allocator, u32 size, AssetManager* asset_manager );
    void                    shutdown();

    ResourcePoolTyped<T>    assets;
    FlatHashMap<u64, T*>    path_to_asset;
    AssetManager*           asset_manager;

}; // struct AssetLoader


struct AssetManager {

    static AssetManager*    init_system();
    static void             shutdown_system( AssetManager* instance );

    void                    set_loader( u32 index, AssetLoaderBase* loader );

    template <typename T>
    T*                      get_loader();

    // Allocate a path inside the 
    AssetPath               allocate_path( StringView path );
    void                    free_path( AssetPath& path );

    ResourcePool            path_string_pool;

    AssetLoaderBase*        loaders[ 32 ];

}; // struct AssetManager

// Implementations ////////////////////////////////////////////////////////

// AssetLoader ////////////////////////////////////////////////////////////
template<typename T>
inline void AssetLoader<T>::init( Allocator* allocator, u32 size, AssetManager* asset_manager_ ) {

    assets.init( allocator, size );
    path_to_asset.init( allocator, size );

    asset_manager = asset_manager_;
}

template<typename T>
inline void AssetLoader<T>::shutdown() {

    assets.shutdown();
    path_to_asset.shutdown();
}

// AssetManager ///////////////////////////////////////////////////////////


template<typename T>
inline T* AssetManager::get_loader() {
    if ( T::k_loader_index <= 32 ) {
        return (T*)( loaders[ T::k_loader_index ] );
    }
    
    return nullptr;
}

} // namespace idra