#version 450

vec2 verticies[3] = 
{
    vec2(-0.5, 0.5),
    vec2(0, -0.5),
    vec2(0.5, 0.5)
};

void main()
{
    gl_Position = vec4(verticies[gl_VertexIndex], 1.0, 1.0);
}
