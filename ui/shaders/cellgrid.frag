#if defined(GL_ES) && __VERSION__ < 320
# extension GL_EXT_shader_io_blocks : require
#endif
precision mediump float;

in Varying {
  smooth mediump float intensity;
} var;

out vec4 outputColor;

void main()
{
  outputColor = vec4(vec3(var.intensity), 1.);
}
