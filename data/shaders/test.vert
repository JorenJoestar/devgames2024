
layout( location = 0 ) in vec2 Position;
layout( std140, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };

void main() {
    gl_Position = vec4( Position.xy,0,1 );
}