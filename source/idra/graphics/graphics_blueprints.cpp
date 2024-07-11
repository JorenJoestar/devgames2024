#include "graphics/graphics_blueprints.hpp"

namespace idra {

// Serializer specializations /////////////////////////////////////////////
#if defined ( IDRA_USE_COMPRESSED_TEXTURES )

template<>
void BlobReader::serialize<TextureBlueprint>( TextureBlueprint* data ) {
    iassert( false ); // TODO: lazy
}

#endif // IDRA_USE_COMPRESSED_TEXTURES

template<>
void BlobReader::serialize<SpriteAnimationCreation>( SpriteAnimationCreation* data ) {

    // TODO:
    //serialize( &data->frame_table_{});
    data->frame_table_ = {};
    blob_read_offset += sizeof( Span<u16> );

    serialize( &data->texture_width );
    serialize( &data->texture_height );
    serialize( &data->offset_x );
    serialize( &data->offset_y );
    serialize( &data->frame_width );
    serialize( &data->frame_height );
    serialize( &data->num_frames );
    serialize( &data->columns );
    serialize( &data->fps );
    serialize( &data->looping );
    serialize( &data->invert );
}

template<>
void BlobReader::serialize<SpriteAnimationBlueprint>( SpriteAnimationBlueprint* data ) {
    serialize( &data->animations );
}


// Atlas blueprints ///////////////////////////////////////////////////////
template<>
void BlobReader::serialize<AtlasEntry>( AtlasEntry* data ) {
    serialize( &data->uv_offset_x );
    serialize( &data->uv_offset_y );
    serialize( &data->uv_width );
    serialize( &data->uv_height );
}

template<>
void BlobReader::serialize<AtlasBlueprint>( AtlasBlueprint* data ) {
    serialize( &data->entries );
    serialize( &data->entry_names );
    serialize( &data->texture_name );
}

// UI blueprint ///////////////////////////////////////////////////////////
template<>
void BlobReader::serialize<UITextFrameEntry>( UITextFrameEntry* data ) {
    serialize( &data->uv_offset_x );
    serialize( &data->uv_offset_y );
    serialize( &data->uv_width );
    serialize( &data->uv_height );
    serialize( &data->position_offset_x );
    serialize( &data->position_offset_y );
}

template<>
void BlobReader::serialize<UIBlueprint>( UIBlueprint* data ) {

    for ( u32 i = 0; i < UIBlueprint::TextFrameElements::Count; ++i ) {
        serialize( &data->text_frame_elements[ i ] );
    }
    
    serialize( &data->entry_names );
    serialize( &data->texture_name );
}


} // namespace idra