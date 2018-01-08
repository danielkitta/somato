uniform mat3x4 modelView;
uniform vec4   viewFrustum;
uniform mat2x4 textureShear;

in vec4 position;
in vec2 normal;

smooth out mediump vec3 varHalfVec;
smooth out mediump vec3 varNormal;
smooth out mediump vec2 varTexcoord;

const vec3 dirToLight = vec3(0., 0.242535625, 0.9701425);

vec3 unwrapOctahedron(vec2 oct)
{
  float z = 1. - abs(oct.x) - abs(oct.y);
  float t = clamp(-z, 0., 1.);
  vec2 xy = oct + mix(vec2(-t), vec2(t), lessThan(oct, vec2(0.)));

  return vec3(xy, z);
}

vec4 project(vec4 frustum, vec3 pos)
{
  return vec4(pos.xy * frustum.xy, pos.z * frustum.z + frustum.w, -pos.z);
}

void main()
{
  vec3 modelNormal = unwrapOctahedron(normal);
  vec3 posCamSpace = position * modelView;

  gl_Position = project(viewFrustum, posCamSpace);
  varHalfVec  = dirToLight - normalize(posCamSpace);
  varNormal   = normalize(modelNormal * mat3(modelView));
  varTexcoord = position * textureShear;
}
