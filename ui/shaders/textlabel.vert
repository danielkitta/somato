#version 150

in vec2 position;
in vec2 texcoord;

smooth out vec2 interpTexcoord;

void main()
{
  gl_Position = vec4(position, 0.0, 1.0);
  interpTexcoord = texcoord;
}
