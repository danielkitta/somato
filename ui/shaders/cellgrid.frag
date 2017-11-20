#ifdef GL_ES
precision mediump float;
#endif

smooth in float interpIntensity;

out vec3 outputColor;

void main()
{
  outputColor = vec3(interpIntensity);
}
