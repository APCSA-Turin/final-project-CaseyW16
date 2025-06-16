#version 430 core
out vec4 FragColor;
uniform vec3 color;
uniform float zoom;

in flat int darken;

void main()
{
    vec3 modColor = color;
    if (darken == 1) {
        modColor *= 0.15f;
    }
    FragColor = vec4(modColor, 0.1f / pow(zoom, 0.25f));
}

