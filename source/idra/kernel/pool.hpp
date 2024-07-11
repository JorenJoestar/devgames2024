/*  
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/array.hpp"
#include "kernel/memory.hpp"

namespace idra {

    constexpr u32                   k_invalid_generation = 0;

    // Index and generation handle, based on talk by Seb Aaltonen
    // https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
    // 
    // Template argument is only to be strongly typed.
    template<typename T>
    struct Handle {

        bool                        is_valid() const    { return generation != 0; }
        bool                        is_invalid() const  { return generation == 0; }

        bool operator== ( const Handle<T>& other ) const { return index == other.index && generation == other.generation; }
        bool operator!= ( const Handle<T>& other ) const { return index != other.index || generation != other.generation; }

        u32                         index       = 0;
        u32                         generation  = 0;
    }; // struct Handle


    // Pool with hot/cold data and per element generation.
    // Will be used to store rendering structures, but could be used in other
    // contexts as well.
    template <typename HotData, typename ColdData, typename HandleType>
    struct Pool {

        void                        init( Allocator* allocator, u32 initial_size );
        void                        shutdown();

        HandleType                  create_object( const ColdData& cold, const HotData& hot );
        HandleType                  obtain_object();
        void                        destroy_object( HandleType handle );

        const ColdData*             get_cold( HandleType handle ) const;
        ColdData*                   get_cold( HandleType handle );

        const HotData*              get_hot( HandleType handle ) const;
        HotData*                    get_hot( HandleType handle );

        Allocator*                  allocator           = nullptr;
        Array<HotData>              hot_data;
        Array<ColdData>             cold_data;
        Array<u32>                  generations;
        Array<u32>                  free_indices;

        u32                         size                = 0;
        u32                         free_indices_head   = 0;

    }; // struct Pool

    //
    //
    struct ResourcePool {

        void                        init( Allocator* allocator, u32 pool_size, u32 resource_size );
        void                        shutdown();

        u32                         obtain_resource();      // Returns an index to the resource
        void                        release_resource( u32 index );
        void                        free_all_resources();

        void*                       access_resource( u32 index );
        const void*                 access_resource( u32 index ) const;

        u8*                         memory          = nullptr;
        u32*                        free_indices    = nullptr;
        Allocator*                  allocator       = nullptr;

        u32                         free_indices_head   = 0;
        u32                         pool_size           = 16;
        u32                         resource_size       = 4;
        u32                         used_indices        = 0;

    }; // struct ResourcePool

    //
    //
    template <typename T>
    struct ResourcePoolTyped : public ResourcePool {

        void                        init( Allocator* allocator, u32 pool_size );
        void                        shutdown();

        T*                          obtain();
        void                        release( T* resource );

        T*                          get( u32 index );
        const T*                    get( u32 index ) const;

    }; // struct ResourcePoolTyped


    // Implementation /////////////////////////////////////////////////////

    // Create an object copying hot and cold data, returns an handle.
    // TODO(gabriel): not sure if Handle<T> should be based on cold-data.
    // TODO(gabriel): maybe implement move semantics ?
    template<typename HotData, typename ColdData, typename HandleType>
    inline void Pool<HotData, ColdData, HandleType>::init( Allocator* allocator_, u32 initial_size ) {
        // TODO:(gabriel): add grow as well!
        allocator = allocator_;
        size = initial_size;

        // Allocate arrays
        hot_data.init( allocator, initial_size, size );
        cold_data.init( allocator, initial_size, size );
        free_indices.init( allocator, initial_size, size );
        generations.init( allocator, initial_size, size );
            
        free_indices_head = 0;

        // Initialize free indices and generations
        for ( u32 i = 0; i < initial_size; ++i ) {
            free_indices[ i ] = i;
            // Start from first generation
            generations[ i ] = 1;
        }
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline void Pool<HotData, ColdData, HandleType>::shutdown() {

        iassert( free_indices_head == 0 );

        hot_data.shutdown();
        cold_data.shutdown();
        generations.shutdown();
        free_indices.shutdown();
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline HandleType Pool<HotData, ColdData, HandleType>::create_object( const ColdData& cold, const HotData& hot ) {
        
        if ( free_indices_head < size ) {
            const u32 free_index = free_indices[ free_indices_head++ ];

            cold_data[ free_index ] = cold;
            hot_data[ free_index ] = hot;
            return { free_index, generations[ free_index ] };
        }
        // Error: no more resources left!
        iassert( false );
        return { 0,0 };
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline HandleType Pool<HotData, ColdData, HandleType>::obtain_object() {
        if ( free_indices_head < size ) {
            const u32 free_index = free_indices[ free_indices_head++ ];
            // Just allocate the handle
            return { free_index, generations[ free_index ] };
        }
        // Error: no more resources left!
        iassert( false );
        return { 0,0 };
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline void Pool<HotData, ColdData, HandleType>::destroy_object( HandleType handle ) {
        
        const u32 index = handle.index;
        // Check validity, helps also with multiple frees!
        if ( handle.generation != generations[ index ] ) {
            return;
        }

        // Increment generation on valid destruction.
        // Generations are also a protection against multiple frees.
        generations[ index ]++;

        // Put the newly free index into the stack (implemented as array + head)
        free_indices[ --free_indices_head ] = index;
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline const ColdData* Pool<HotData, ColdData, HandleType>::get_cold( HandleType handle ) const {
        const u32 index = handle.index;
        // Check validity
        if ( handle.generation != generations[ index ] ) {
            return nullptr;
        }

        return &cold_data[ index ];
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline ColdData* Pool<HotData, ColdData, HandleType>::get_cold( HandleType handle ) {
        const u32 index = handle.index;
        // Check validity
        if ( handle.generation != generations[ index ] ) {
            return nullptr;
        }

        return &cold_data[ index ];
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline const HotData* Pool<HotData, ColdData, HandleType>::get_hot( HandleType handle ) const {
        const u32 index = handle.index;
        // Check validity
        if ( handle.generation != generations[ index ] ) {
            return nullptr;
        }

        return &hot_data[ index ];
    }

    template<typename HotData, typename ColdData, typename HandleType>
    inline HotData* Pool<HotData, ColdData, HandleType>::get_hot( HandleType handle ) {
        const u32 index = handle.index;
        // Check validity
        if ( handle.generation != generations[ index ] ) {
            return nullptr;
        }

        return &hot_data[ index ];
    }

    // ResourcePoolTyped //////////////////////////////////////////////////

    template<typename T>
    inline void ResourcePoolTyped<T>::init( Allocator* allocator_, u32 pool_size_ ) {
        ResourcePool::init( allocator_, pool_size_, sizeof( T ) );
    }

    template<typename T>
    inline void ResourcePoolTyped<T>::shutdown() {
        if ( free_indices_head != 0 ) {
            ilog_warn( "Resource pool has unfreed resources.\n" );

            for ( u32 i = 0; i < free_indices_head; ++i ) {
                //ilog_warn( "\tResource %u, %s\n", free_indices[ i ], get( free_indices[ i ] )->name.data );
            }
        }
        ResourcePool::shutdown();
    }

    template<typename T>
    inline T* ResourcePoolTyped<T>::obtain() {
        u32 resource_index = ResourcePool::obtain_resource();
        if ( resource_index != u32_max ) {
            T* resource = get( resource_index );
            resource->pool_index = resource_index;
            return resource;
        }

        return nullptr;
    }

    template<typename T>
    inline void ResourcePoolTyped<T>::release( T* resource ) {
        ResourcePool::release_resource( resource->pool_index );
    }

    template<typename T>
    inline T* ResourcePoolTyped<T>::get( u32 index ) {
        return ( T* )ResourcePool::access_resource( index );
    }

    template<typename T>
    inline const T* ResourcePoolTyped<T>::get( u32 index ) const {
        return ( const T* )ResourcePool::access_resource( index );
    }

} // namespace idra