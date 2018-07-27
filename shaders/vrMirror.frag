#version 330 core

uniform sampler2D MirrorBuffer;
uniform vec2 ScreenSize;

out vec4 FragColor;

vec2 calcTexCoord()
{
	vec2 origTexCoord = gl_FragCoord.xy / ScreenSize;

    // flip due to mirror being upside down
    return vec2(origTexCoord.x, 1.0 - origTexCoord.y);
}

void main()
{
	FragColor = texture(MirrorBuffer, calcTexCoord());
}