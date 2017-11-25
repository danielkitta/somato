#ifdef GL_ES
precision mediump float;
#endif

smooth in float varIntensity;

out vec3 outputColor;

void main()
{
  outputColor = vec3(varIntensity);
}
