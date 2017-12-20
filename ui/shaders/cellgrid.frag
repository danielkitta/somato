#if defined(GL_ES) && __VERSION__ < 320
# extension GL_EXT_shader_io_blocks : require
#endif
#ifdef GL_ES
precision mediump float;
#endif

in Varying {
  smooth float intensity;
} var;

out vec3 outputColor;

void main()
{
  outputColor = vec3(var.intensity);
}
