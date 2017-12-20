#if defined(GL_ES) && __VERSION__ < 320
# extension GL_EXT_shader_io_blocks : require
#endif

uniform mat3x4 modelView;
uniform vec4   viewFrustum;

in vec4 position;

out Vertex {
  vec2  devCoord;
  float intensity;
} v_out;

const float gridLuminance = 0.3;

vec4 project(vec4 frustum, vec3 pos)
{
  return vec4(pos.xy * frustum.xy, pos.z * frustum.z + frustum.w, -pos.z);
}

float depthFade(float z)
{
  return clamp(0.0625 * z + 1., 0., 1.);
}

void main()
{
  vec3 posCamSpace = position * modelView;
  vec4 clipPos     = project(viewFrustum, posCamSpace);

  gl_Position     = clipPos;
  v_out.devCoord  = 1. / clipPos.w * clipPos.xy;
  v_out.intensity = gridLuminance * depthFade(posCamSpace.z);
}
