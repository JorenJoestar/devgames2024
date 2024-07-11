
layout (location = 0) in vec2 uv;
layout (location = 1) flat in uint global_data_index;

layout (location = 0) out vec4 out_color;

void main() {

	vec4 char_data = data[global_data_index];
	vec2 duv = uv * CHAR_SIZE;
	vec2 print_pos = vec2(0, 10);
    float textPixel = print_char(char_data, duv, print_pos);
    
    if (textPixel < 0.01f)
        discard;
    
    vec3 col = vec3(1);
	col *= mix(vec3(0.2),vec3(0.5,1,0),textPixel);
    out_color = vec4(col.rgb, 1);
}