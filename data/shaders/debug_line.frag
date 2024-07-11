
layout (location = 0) in vec4 Frag_Color;
layout (location = 1) in vec2 Frag_UV;

layout (location = 0) out vec4 Out_Color;

float saturate(float v) {
    return clamp(v, 0.0, 1.0);
}

void main()
{
    vec4 col = Frag_Color;
    //float alpha_u = saturate(1 - abs(Frag_UV.x * 0.7 - 0.35));
    float alpha_v = saturate(1 - abs(Frag_UV.y * 2 - 1));
    col.a *= alpha_v;
    
    Out_Color = col;
}