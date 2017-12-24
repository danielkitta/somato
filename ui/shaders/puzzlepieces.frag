precision mediump float;

uniform sampler2D pieceTexture;
uniform vec4      diffuseColor;

smooth in mediump vec3 varHalfVec;
smooth in mediump vec3 varNormal;
smooth in mediump vec2 varTexcoord;

out vec4 outputColor;

const vec3  dirToLight     = vec3(0., 0.242535625, 0.9701425);
const float lightIntensity = 0.8;
const float ambIntensity   = 0.05;
const float specIntensity  = 0.04;
const float shininess      = 12.;

float inverseLength(vec3 v)
{
  return inversesqrt(dot(v, v));
}

void main()
{
  float texIntensity = texture(pieceTexture, varTexcoord).r;

  float rMagNormal   = inverseLength(varNormal);
  float rMagHalfVec  = inverseLength(varHalfVec);
  float dotNormLight = dot(varNormal, dirToLight);
  float dotNormHalf  = dot(varNormal, varHalfVec);
  float cosIncidence = clamp(dotNormLight * rMagNormal, 0., 1.);
  float cosHalfIncid = clamp(dotNormHalf * rMagNormal * rMagHalfVec, 0., 1.);

  float specHighlight = pow(cosHalfIncid, shininess);
  float specularTerm  = specHighlight * specIntensity * cosIncidence;
  float diffuseTerm   = texIntensity * (lightIntensity * cosIncidence + ambIntensity);

  outputColor = vec4(diffuseColor.rgb * diffuseTerm + specularTerm, 1.);
}
