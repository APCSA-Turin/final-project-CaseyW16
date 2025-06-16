#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aOffset;
layout (location = 3) in int highlighted;
uniform float zoom;
uniform vec2 camOffset;

flat out int highlight;
out vec2 pos;

void main()
{
	gl_Position = vec4(aPos * pow(zoom, 0.5f) + aOffset - camOffset, 0.0, zoom);
	pos = vec2(aPos.x, aPos.y);
	highlight = highlighted;
}