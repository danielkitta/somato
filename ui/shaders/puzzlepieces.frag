#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D pieceTexture;
uniform vec4 diffuseMaterial;

smooth in vec3 varHalfVec;
smooth in vec3 varNormal;
smooth in vec2 varTexcoord;

out vec3 outputColor;

const vec3  dirToLight     = vec3(0., 0.242535625, 0.9701425);
const float lightIntensity = 0.7;
const float ambIntensity   = 0.2;
const float specIntensity  = 0.1;
const float shininess      = 32.;

void main()
{
  float texIntensity = texture(pieceTexture, varTexcoord).r;

  float rMagNormal   = inversesqrt(dot(varNormal, varNormal));
  float rMagHalfVec  = inversesqrt(dot(varHalfVec, varHalfVec));
  float dotNormLight = dot(varNormal, dirToLight);
  float dotNormHalf  = dot(varNormal, varHalfVec);
  float cosIncidence = clamp(dotNormLight * rMagNormal, 0., 1.);
  float cosHalfIncid = clamp(dotNormHalf * rMagNormal * rMagHalfVec, 0., 1.);

  float specHighlight = pow(cosHalfIncid, shininess) * specIntensity;
  float specularTerm  = (cosIncidence != 0.) ? specHighlight : 0.;
  float diffuseTerm   = texIntensity * (lightIntensity * cosIncidence + ambIntensity);

  outputColor = diffuseMaterial.rgb * diffuseTerm + specularTerm;
}
