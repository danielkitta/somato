#version 150

uniform vec2 windowSize;

in vec2 position;
in vec2 texcoord;

smooth out vec2 interpTexcoord;

void main()
{
  vec2 posClipSpace = position / windowSize * 2.0 - vec2(1.0);

  gl_Position = vec4(posClipSpace, 0.0, 1.0);
  interpTexcoord = texcoord;
}
