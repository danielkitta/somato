#ifdef GL_ES
precision mediump float;
#endif

smooth in vec3 varColor;

out vec3 outputColor;

void main()
{
  outputColor = varColor;
}
