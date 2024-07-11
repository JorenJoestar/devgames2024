
#include "kernel/blob.hpp"

namespace idra {

// BlobWriter /////////////////////////////////////////////////////////////
char* BlobWriter::reserve( sizet size ) {
    if ( reserved_offset + size > total_size ) {
        ilog_debug( "Blob allocation error: reserved, requested, total - %u + %u > %u\n", reserved_offset, size, total_size );
        return nullptr;
    }

    u32 offset = reserved_offset;
    reserved_offset += ( u32 )size;

    return blob_destination_memory + offset;
}

void BlobWriter::reserve_and_set( RelativeString& data, const StringView string_data ) {
    char* destination_memory = reserve( string_data.size + 1 );
    data.set( destination_memory, (u32)string_data.size );
    memcpy( destination_memory, string_data.data, string_data.size );
    // Add null termination
    destination_memory[ string_data.size ] = 0;
}

// NOTE: this is really error prone, as serialization of element type
// could break if subsequent elements are not allocated properly.
//template<typename T>
//void reserve_and_set( RelativeArray<T>& data, Span<T>& source ) {
//    u32 cached_write_offset = write_offset;
//    // Move serialization to the newly allocated memory,
//    // at the end of the blob.
//    write_offset = reserved_offset;

//    char* destination_memory = reserve( sizeof( T ) * source.size );
//    data.set( destination_memory, source.size );

//    for ( u32 i = 0; i < data.size; ++i ) {
//        T* source_data = &source[ i ];
//        serialize<T>( source_data );
//    }

//    // Restore
//    write_offset = cached_write_offset;
//}

void BlobWriter::serialize( u32* data ) {
    memcpy( &blob_destination_memory[ write_offset ], data, sizeof( u32 ) );

    write_offset += sizeof( u32 );
}

void BlobWriter::serialize( i32* data ) {
    memcpy( &blob_destination_memory[ write_offset ], data, sizeof( i32 ) );

    write_offset += sizeof( i32 );
}

void BlobWriter::serialize( f32* data ) {
    memcpy( &blob_destination_memory[ write_offset ], data, sizeof( f32 ) );

    write_offset += sizeof( f32 );
}

// BlobReader /////////////////////////////////////////////////////////////

char* BlobReader::reserve_static( sizet size ) {
    if ( reserved_offset + size > blob_source_memory.size ) {
        ilog_debug( "Blob allocation error: reserved, requested, total - %u + %u > %u\n", reserved_offset, size, blob_source_memory.size );
        return nullptr;
    }

    u32 offset = reserved_offset;
    reserved_offset += ( u32 )size;

    return data_memory + offset;
}

i32 BlobReader::get_relative_data_offset( void* data ) {
    // data_memory points to the newly allocated data structure to be used at runtime.
    const i32 data_offset_from_start = ( i32 )( ( char* )data - data_memory );
    const i32 data_offset = reserved_offset - data_offset_from_start;
    return data_offset;
}

void BlobReader::serialize( RelativeString* data ) {
    // Blob --> Data
    serialize( &data->size );
    // Original data source offset. Could be different than current data offset.
    i32 source_data_offset;
    serialize( &source_data_offset );

    if ( source_data_offset > 0 ) {
        // Calculate correct data offset. Needed because of versioning.
        data->data.offset = get_relative_data_offset( data ) - sizeof( u32 );

        // Reserve memory + string ending
        reserve_static( ( sizet )data->size + 1 );

        // Manually resolve the memory - source data offset is relative to the original reading
        // variable position in memory, and make it point at the beginning of the string.
        char* source_data = blob_source_memory.data + blob_read_offset + source_data_offset - sizeof( u32 );
        memcpy( ( char* )data->c_str(), source_data, ( sizet )data->size + 1 );
        ilog_debug( "Found %s\n", data->c_str() );

    } else {
        data->set_empty();
    }
}


void BlobReader::serialize( char* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( char ) );

    blob_read_offset += sizeof( char );
}

void BlobReader::serialize( i8* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( i8 ) );

    blob_read_offset += sizeof( i8 );
}

void BlobReader::serialize( u8* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( u8 ) );

    blob_read_offset += sizeof( u8 );
}

void BlobReader::serialize( i16* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( i16 ) );

    blob_read_offset += sizeof( i16 );
}

void BlobReader::serialize( u16* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( u16 ) );

    blob_read_offset += sizeof( u16 );
}

void BlobReader::serialize( i32* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( i32 ) );

    blob_read_offset += sizeof( i32 );
}

void BlobReader::serialize( u32* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( u32 ) );

    blob_read_offset += sizeof( u32 );
}

void BlobReader::serialize( i64* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( i64 ) );

    blob_read_offset += sizeof( i64 );
}

void BlobReader::serialize( u64* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( u64 ) );

    blob_read_offset += sizeof( u64 );
}

void BlobReader::serialize( f32* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( f32 ) );

    blob_read_offset += sizeof( f32 );
}

void BlobReader::serialize( f64* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( f64 ) );

    blob_read_offset += sizeof( f64 );
}

void BlobReader::serialize( bool* data ) {
    memcpy( data, &blob_source_memory.data[ blob_read_offset ], sizeof( bool ) );

    blob_read_offset += sizeof( bool );
}


} // namespace idra