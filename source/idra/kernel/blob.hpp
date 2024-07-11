#pragma once

#include "kernel/relative_data_structures.hpp"
#include "kernel/allocator.hpp"
#include "kernel/memory.hpp"
#include "kernel/assert.hpp"

namespace idra {


struct BlobHeader {
    u32                 version;
    u32                 mappable;
}; // struct BlobHeader

struct Blob {
    BlobHeader          header;
}; // struct Blob

// Serializers
struct BlobWriter {

    template <typename T>
    T*              write( Allocator* allocator_, u32 serializer_version, sizet size );

    char*           reserve( sizet size );

    void            reserve_and_set( RelativeString& data, const StringView string_data );

    template<typename T>
    void            reserve_and_set( RelativeArray<T>& data, u32 num_elements );

    // NOTE: this is really error prone, as serialization of element type
    // could break if subsequent elements are not allocated properly.
    //template<typename T>
    //void          reserve_and_set( RelativeArray<T>& data, Span<T>& source );

    void            serialize( u32* data );
    void            serialize( i32* data );
    void            serialize( f32* data );

    template<typename T>
    void            serialize( T* data );

    Allocator*      allocator               = nullptr;
    char*           blob_destination_memory = nullptr;

    u32             write_offset    = 0;
    u32             reserved_offset = 0;
    u32             total_size      = 0;

}; // struct BlobWriter


/// <summary>
/// 
/// Class that reads from a blob of memory. If the blob and the serializer have
/// different versions, manually serialize the data, allocating memory.
/// 
/// </summary>
struct BlobReader {

    template<typename T>
    T*              read( Allocator* allocator_, u32 serializer_version_, 
                          Span<char> blob_memory, bool force_serialization );

    char*           reserve_static( sizet size );

    template<typename T>
    T*              reserve_static();

    // Generic method to specialize when reading custom structures,
    // in a similar form:
    // 
    // (in a header file)
    // template<>
    // void BlobReader::serialize<CustomType>( CustomType* data );
    // 
    // (in a source file)
    // template<>
    // void BlobReader::serialize<CustomType>( CustomType* data ) {
        //serialize( &data->variable0 );
    // }
    //
    // Separation of declaration and implementation is needed to work
    // across multiple executable/dlls.
    // https://stackoverflow.com/questions/1481677/how-to-provide-a-explicit-specialization-to-only-one-method-in-a-c-template-cl/1481796#1481796

    template<typename T>
    void            serialize( T* data );

    void            serialize( RelativeString* data );

    template<typename T>
    void            serialize( RelativeArray<T>* data );

    void            serialize( char* data );
    void            serialize( i8* data );
    void            serialize( u8* data );
    void            serialize( i16* data );
    void            serialize( u16* data );
    void            serialize( i32* data );
    void            serialize( u32* data );
    void            serialize( i64* data );
    void            serialize( u64* data );
    void            serialize( f32* data );
    void            serialize( f64* data );
    void            serialize( bool* data );

    i32             get_relative_data_offset( void* data );

    Span<char>      blob_source_memory;

    Allocator*      allocator           = nullptr;
    char*           data_memory         = nullptr;      // Used when serializing instead of just casting,
                                                        // either for different serializer versions or
                                                        // if forced on read.

    u32             blob_read_offset    = 0;
    u32             reserved_offset     = 0;

    u32             serializer_version  = 0xffffffff;   // Version coming from the code.
    u32             data_version        = 0xffffffff;   // Version read from blob or written into blob.

}; // struct BlobReader

// Implementations ////////////////////////////////////////////////////////

// BlobWriter /////////////////////////////////////////////////////////////
template<typename T>
inline T* BlobWriter::write( Allocator* allocator_, u32 serializer_version, sizet size ) {

    allocator = allocator_;

    blob_destination_memory = ( char* )ialloc( size + sizeof( BlobHeader ), allocator );

    total_size = ( u32 )size + sizeof( BlobHeader );
    write_offset = reserved_offset = 0;

    // Write header
    BlobHeader* header = ( BlobHeader* )reserve( sizeof( BlobHeader ) );
    header->version = serializer_version;

    write_offset = reserved_offset;

    reserve( sizeof( T ) - sizeof( BlobHeader ) );

    return ( T* )blob_destination_memory;
}

template<typename T>
inline void BlobWriter::reserve_and_set( RelativeArray<T>& data, u32 num_elements ) {
    char* destination_memory = reserve( sizeof( T ) * num_elements );
    data.set( destination_memory, num_elements );
}

template<typename T>
inline void BlobWriter::serialize( T* data ) {
    // Should not arrive here!
    iassertm( false, "Specialization of Type is necessary to properly write data.\n" );
}

// BlobReader /////////////////////////////////////////////////////////////

template<typename T>
inline T* BlobReader::read( Allocator* allocator_, u32 serializer_version_, 
                            Span<char> blob_memory, bool force_serialization ) {

    allocator = allocator_;
    blob_source_memory = blob_memory;
    data_memory = nullptr;

    blob_read_offset = reserved_offset = 0;

    serializer_version = serializer_version_;
    // is_reading = 1;
     //has_allocated_memory = 0;

     // Read header from blob.
    BlobHeader* header = ( BlobHeader* )blob_source_memory.data;
    data_version = header->version;
    //is_mappable = header->mappable;

    // If serializer and data are at the same version, no need to serialize.
    // TODO: is mappable should be taken in consideration.
    if ( serializer_version == data_version && !force_serialization ) {
        return ( T* )( blob_source_memory.data );
    }

    ilog_debug( "Serializer is different version - serialize and allocate.\n" );

    // has_allocated_memory = 1;
    serializer_version = data_version;

    // Allocate data
    data_memory = ( char* )ialloc( blob_memory.size, allocator );
    T* destination_data = ( T* )data_memory;
    // Move reading after the header
    blob_read_offset += sizeof( BlobHeader );

    reserve_static( sizeof( T ) );
    // Read from blob to data
    serialize( destination_data );

    return destination_data;
}

template<typename T>
inline T* BlobReader::reserve_static() {
    return ( T* )reserve_static( sizeof( T ) );
}

template<typename T>
inline void BlobReader::serialize( T* data ) {
    // Should not arrive here!
    iassertm( false, "Specialization of Type is necessary to properly read data.\n" );
}

template<typename T>
inline void BlobReader::serialize( RelativeArray<T>* data ) {

    // Blob --> Data
    serialize( &data->size );
    // Original data source offset. Could be different than current data offset.
    i32 source_data_offset;
    serialize( &source_data_offset );

    // Cache read offset
    u32 cached_read_offset = blob_read_offset;
    // Calculate correct data offset. Needed because of versioning.
    data->data.offset = get_relative_data_offset( data ) - sizeof( u32 );

    // Reserve memory
    reserve_static( data->size * sizeof( T ) );
    // Move read offset to correct blob read location
    blob_read_offset = cached_read_offset + source_data_offset - sizeof( u32 );
    // Read each element
    for ( u32 i = 0; i < data->size; ++i ) {
        T* destination = &data->get()[ i ];
        serialize( destination );

        destination = destination;
    }
    // Restore read offset
    blob_read_offset = cached_read_offset;
}

// NOTE: this is really error prone, as serialization of element type
// could break if subsequent elements are not allocated properly.
//template<typename T>
//inline void reserve_and_set( RelativeArray<T>& data, Span<T>& source ) {
//    u32 cached_write_offset = write_offset;
//    // Move serialization to the newly allocated memory,
//    // at the end of the blob.
//    write_offset = reserved_offset;

//    char* destination_memory = reserve( sizeof( T ) * source.size_ );
//    data.set( destination_memory, source.size_ );

//    for ( u32 i = 0; i < data.size; ++i ) {
//        T* source_data = &source[ i ];
//        serialize<T>( source_data );
//    }

//    // Restore
//    write_offset = cached_write_offset;
//}

} // namespace idra