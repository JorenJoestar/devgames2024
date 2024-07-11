
#include "gpu_resources.hpp"

namespace idra {
namespace GpuUtils {

size_t calculate_texture_size( Texture* texture ) {
    size_t texture_size = texture->width * texture->height * texture->depth;
    iassert( texture->mip_level_count == 1 );

    switch ( texture->format ) {
        case TextureFormat::R32G32B32A32_FLOAT:
        {
            texture_size *= sizeof( f32 ) * 4;
        } break;

        case TextureFormat::R16G16B16A16_FLOAT:
        {
            texture_size *= sizeof( u16 ) * 4;
        } break;

        case TextureFormat::R8G8B8A8_UNORM:
        {
            texture_size *= sizeof( u8 ) * 4;
        } break;

        case TextureFormat::R8_UNORM:
        {
            texture_size *= sizeof(u8);
        } break;

        default:
        {
            iassert( !"Not supported" );
            return 0;
        }
    }

    return texture_size;
}

} // namespace GpuUtils
} // namespace idra
