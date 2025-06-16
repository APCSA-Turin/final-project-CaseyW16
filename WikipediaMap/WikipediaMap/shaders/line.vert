#version 430 core
layout (location = 2) in vec2 offset;
uniform float zoom;
uniform vec2 camOffset;
uniform int searching;

out flat int darken;

void main()
{
    gl_Position = vec4(offset - camOffset, 0.0, zoom);
    darken = int(searching);
}

