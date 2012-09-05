#version 150

uniform vec2 windowSize;

in vec2 position;

void main()
{
  vec2 posClipSpace = (position / windowSize) * 2.0 - 1.0;

  gl_Position = vec4(posClipSpace, 0.0, 1.0);
}
