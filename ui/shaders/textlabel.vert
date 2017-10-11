uniform float textIntensity;

in vec2 position;
in vec2 texcoord;
in vec3 color;

noperspective out vec2 interpTexcoord;
flat          out vec3 interpColor;

void main()
{
  gl_Position    = vec4(position, 0., 1.);
  interpTexcoord = texcoord;
  interpColor    = color * textIntensity;
}
