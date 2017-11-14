uniform sampler2D pieceTexture;
uniform vec4 diffuseMaterial;

smooth in vec3 interpHalfVec;
smooth in vec3 interpNormal;
smooth in vec2 interpTexcoord;

out vec3 outputColor;

const vec3  dirToLight     = vec3(0., 0.242535625, 0.9701425);
const float lightIntensity = 0.7;
const float ambIntensity   = 0.2;
const float specIntensity  = 0.1;
const float shininess      = 32.;

void main()
{
  float texIntensity = texture(pieceTexture, interpTexcoord).r;

  float normalSquare  = dot(interpNormal, interpNormal);
  float halfVecSquare = dot(interpHalfVec, interpHalfVec);
  float rsqrtNormal   = inversesqrt(normalSquare);
  float rsqrtNormHalf = inversesqrt(normalSquare * halfVecSquare);
  float dotNormLight  = dot(interpNormal, dirToLight);
  float dotNormHalf   = dot(interpNormal, interpHalfVec);

  float cosIncidence = clamp(dotNormLight * rsqrtNormal, 0., 1.);
  float spec = pow(clamp(dotNormHalf * rsqrtNormHalf, 0., 1.), shininess) * specIntensity;
  float specTerm = (cosIncidence != 0.) ? spec : 0.;
  float diffuseTerm = texIntensity * (lightIntensity * cosIncidence + ambIntensity);

  outputColor = diffuseMaterial.rgb * diffuseTerm + specTerm;
}
