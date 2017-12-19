#if defined(GL_ES) && __VERSION__ < 320
# extension GL_OES_shader_io_blocks : require
#endif

uniform mat3x4 modelView;
uniform vec4   viewFrustum;
uniform vec2   windowSize;

in vec4 position;
in vec2 normal;

out Vertex {
  vec2 winPos;
  vec3 halfVec;
  vec3 normal;
} v_out;

const vec3 dirToLight = vec3(0., 0.242535625, 0.9701425);

vec4 unwrapOctahedron(vec2 oct)
{
  float z = 1. - abs(oct.x) - abs(oct.y);
  float t = clamp(-z, 0., 1.);
  vec2 xy = oct + mix(vec2(-t), vec2(t), lessThan(oct, vec2(0.)));

  return vec4(xy, z, 0.);
}

vec4 project(vec4 frustum, vec3 pos)
{
  return vec4(pos.xy * frustum.xy, pos.z * frustum.z + frustum.w, -pos.z);
}

void main()
{
  vec4 modelNormal = unwrapOctahedron(normal);
  vec3 posCamSpace = position * modelView;
  vec4 clipPos     = project(viewFrustum, posCamSpace);

  gl_Position   = clipPos;
  v_out.winPos  = 1. / clipPos.w * clipPos.xy * windowSize;
  v_out.halfVec = dirToLight - normalize(posCamSpace);
  v_out.normal  = normalize(modelNormal * modelView);
}
