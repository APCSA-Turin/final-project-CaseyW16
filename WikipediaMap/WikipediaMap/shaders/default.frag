#version 430 core

#define Radius float(0.01)
#define Fade float(.01)

uniform float zoom;

out vec4 fragColor;
in vec2 pos;
flat in int highlight;

vec3 DrawCircle(vec2 uv, float radius, float fade)
{
    float uvLength = length(uv);
    vec3 circle = vec3(smoothstep(radius, radius - fade, uvLength));
    return vec3(circle);
}

void main()
{
    vec2 uv = pos;
    float r = Radius;
    float f = Fade;
    if (highlight == 2) {
        r *= 4;
        f *= 5;
    }
    vec3 circle = DrawCircle(uv.xy, r, f + zoom * 0.00003f);
    if (highlight == 1) {
        circle *= 0.1f;
    } else if (highlight == 2) {
        circle.z = 0;
    }
    fragColor = vec4(circle, 0.0);
}