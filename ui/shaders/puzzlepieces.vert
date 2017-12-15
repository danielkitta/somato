uniform mat3x4 modelView;
uniform vec4   viewFrustum;
uniform mat2x4 textureShear;

in vec4 position;
in vec2 normal;

smooth out vec3 varHalfVec;
smooth out vec3 varNormal;
smooth out vec2 varTexcoord;

const vec3 dirToLight = vec3(0., 0.242535625, 0.9701425);

vec4 unwrapOctahedron(vec2 oct)
{
  float z = 1. - abs(oct.x) - abs(oct.y);
  float t = clamp(-z, 0., 1.);
  vec2 xy = oct + mix(vec2(-t), vec2(t), lessThan(oct, vec2(0.)));

  return vec4(xy, z, 0.);
}

void main()
{
  vec4 modelNormal = unwrapOctahedron(normal);
  vec3 posCamSpace = position * modelView;

  varHalfVec  = dirToLight - normalize(posCamSpace);
  varNormal   = normalize(modelNormal * modelView);
  varTexcoord = position * textureShear;

  gl_Position = vec4(posCamSpace.xy * viewFrustum.xy,
                     posCamSpace.z  * viewFrustum.z + viewFrustum.w,
                    -posCamSpace.z);
}
