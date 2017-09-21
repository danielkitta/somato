uniform sampler2D pieceTexture;
uniform vec4 diffuseMaterial;

smooth in vec3 interpPosition;
smooth in vec3 interpNormal;
smooth in vec2 interpTexcoord;

out vec3 outputColor;

const vec3  dirToLight     = vec3(0., 0.242535625, 0.9701425);
const float lightIntensity = 0.8;
const float ambIntensity   = 0.2;
const float specIntensity  = 0.1;

void main()
{
  vec3 normNormal = normalize(interpNormal);
  float cosIncidence = clamp(dot(normNormal, dirToLight), 0., 1.);

  vec3 viewDir = normalize(interpPosition);
  vec3 halfVec = normalize(dirToLight - viewDir);

  float blinnTerm = pow(clamp(dot(normNormal, halfVec), 0., 1.), 32.);
  float specReflection = (cosIncidence != 0.) ? blinnTerm : 0.;

  float texIntensity = 0.5 * texture(pieceTexture, interpTexcoord).r + 0.3;
  float diffuseTerm = texIntensity * (lightIntensity * cosIncidence + ambIntensity);

  outputColor = diffuseMaterial.rgb * diffuseTerm + specIntensity * specReflection;
}
