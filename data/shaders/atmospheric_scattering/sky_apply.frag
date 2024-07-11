
layout (location = 0) in vec2 vTexCoord;
layout (location = 1) flat in uint albedo_id;

layout (location = 0) out vec4 out_color;

layout (std140, set = 1, binding=0)
uniform Local{
    AtmosphereParameters atmosphere_params;
};

void main() {
    vec3 scene_color = vec3(0);
    float opacity = 0;
    float depth = texture_bindless_2d( atmosphere_params.scene_depth_texture_index, vTexCoord.xy ).r;
    vec3 world_direction;
    vec3 earth_position = earth_position_from_uv_depth( vTexCoord.xy, 1.f, atmosphere_params, world_direction );

    vec2 uv = vTexCoord.xy;
    vec4 H = vec4( uv.x * 2 - 1, uv.y * -2 + 1, 1, 1);
    vec4 HViewPos = atmosphere_params.inverse_projection * H;
    world_direction = normalize(mat3(atmosphere_params.inverse_view) * (HViewPos.xyz / HViewPos.w)).xyz;

    earth_position = earth_position_from_world( atmosphere_params.camera_position, atmosphere_params );
    float viewHeight = length(earth_position);

    if ( depth == 1.0f )
    {
        float2 uv;
        float3 UpVector = earth_position / viewHeight;
        float viewZenithCosAngle = dot(world_direction, UpVector);

        float3 sideVector = normalize(cross(UpVector, world_direction));       // assumes non parallel vectors
        float3 forwardVector = normalize(cross(sideVector, UpVector));  // aligns toward the sun light but perpendicular to up vector
        float2 lightOnPlane = float2(dot(atmosphere_params.sun_direction, forwardVector), dot(atmosphere_params.sun_direction, sideVector));
        lightOnPlane = normalize(lightOnPlane);
        float lightViewCosAngle = lightOnPlane.x;

        bool IntersectGround = raySphereIntersectNearest(earth_position, world_direction, float3(0, 0, 0), atmosphere_params.bottom_radius) >= 0.0f;

        SkyViewLutParamsToUv(atmosphere_params, world_direction, IntersectGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

        scene_color = texture_bindless_2d( atmosphere_params.sky_view_lut_texture_index, uv ).rgb;

        if (!IntersectGround) {
            scene_color += GetSunLuminance2(atmosphere_params, earth_position, world_direction, atmosphere_params.bottom_radius, atmosphere_params.sun_direction);
        }
        
        opacity = 1.0f;
    }
    else {

#define USE_AERIAL_PERSPECTIVE
#if defined (USE_AERIAL_PERSPECTIVE)
        vec3 pixel_world_position = world_position_from_depth( vTexCoord.xy, depth, atmosphere_params.inverse_view_projection );
        float tDepth = length(pixel_world_position.xyz - atmosphere_params.camera_position);
        float Slice = AerialPerspectiveDepthToSlice(tDepth);
        float Weight = 1.0;
        if (Slice < 0.5)
        {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            Weight = saturate(Slice * 2.0);
            Slice = 0.5;
        }
        float w = sqrt(Slice / (AP_SLICE_COUNT)); // squared distribution
        float4 aerial_perspective = Weight * texture_bindless_3d( atmosphere_params.aerial_perspective_texture_index, vec3(vTexCoord.xy, w) );
       
        //scene_color += aerial_perspective.rgb;
        //opacity = aerial_perspective.a;
        //aerial_perspective.a = clamp(aerial_perspective.a * 1.5, 0, 1);
        //scene_color.rgb = scene_color.rgb * (1 - aerial_perspective.a) + aerial_perspective.rgb * aerial_perspective.a;//+ aerial_perspective.rgb * (1 - aerial_perspective.a));
#else
        // Move to top atmosphere as the starting point for ray marching.
        // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
        if (!MoveToTopAtmosphere(earth_position, world_direction, atmosphere_params.top_radius))
        {
            // Ray is not intersecting the atmosphere       
            scene_color = GetSunLuminance(earth_position, world_direction, atmosphere_params.bottom_radius, atmosphere_params.sun_direction);
            opacity = 1.0f;
        }
        else 
        {
            vec3 pixel_world_position = world_position_from_depth( vTexCoord.xy, depth, atmosphere_params.inverse_view_projection );
            const vec2 texture_size = texture_bindless_size2d( atmosphere_params.scene_depth_texture_index ) * 1.0f;
            const bool ground = false;
            const float SampleCountIni = 0.0f;
            const bool VariableSampleCount = true;
            const bool MieRayPhase = true;
            uint transmittance_lut = atmosphere_params.transmittance_lut_texture_index;
            SingleScatteringResult ss = IntegrateScatteredLuminance(gl_FragCoord.xy, earth_position, world_direction, atmosphere_params.sun_direction, atmosphere_params, ground, 
                                        SampleCountIni, depth, VariableSampleCount, MieRayPhase, T_RAY_MAX,
                                        texture_size, transmittance_lut);
            const float Transmittance = dot(ss.Transmittance, vec3(1.0 / 3.0));

            scene_color = ss.L / 1;

            //scene_color = vec3(1 - Transmittance);
            opacity = 1 - Transmittance;
            //opacity = 1.0f;
        }

        

#endif // USE_AERIAL_PERSPECTIVE
    }

    // Similar setup to the Bruneton demo
    vec3 white_point = vec3(1.08241, 0.96756, 0.95003);
    float exposure = 10.0;
    scene_color = pow( vec3(1) - exp(-scene_color / white_point * exposure), vec3(1/2.2));
    //return float4( pow((float3) 1.0 - exp(-rgbA.rgb / white_point * exposure), (float3)(1.0 / 2.2)), 1.0 );

    out_color = vec4( scene_color, opacity );
}