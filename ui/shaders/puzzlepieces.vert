uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;

in vec3 position;
in vec3 normal;

smooth out vec3 interpHalfVec;
smooth out vec3 interpNormal;
smooth out vec2 interpTexcoord;

const mat3x2 texShear = mat3x2( 0.474773,   0.00168634,
                                0.0146367, -0.0145917,
                               -0.0012365,  0.474773);
const vec2 texOffset  = vec2(0.74, 0.26);
const vec3 dirToLight = vec3(0., 0.242535625, 0.9701425);

void main()
{
  vec4 posCamSpace  = modelToCameraMatrix * vec4(position, 1.);
  vec4 normCamSpace = modelToCameraMatrix * vec4(normal, 0.);

  interpHalfVec  = dirToLight - normalize(posCamSpace.xyz);
  interpNormal   = normCamSpace.xyz;
  interpTexcoord = texShear * position + texOffset;

  gl_Position = cameraToClipMatrix * vec4(posCamSpace.xyz, 1.);
}
