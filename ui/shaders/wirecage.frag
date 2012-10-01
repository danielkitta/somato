#version 150

smooth in float interpIntensity;

out vec3 outputColor;

void main()
{
  outputColor = interpIntensity.rrr;
}
