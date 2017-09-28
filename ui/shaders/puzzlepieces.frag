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
  float texIntensity = 0.5 * texture(pieceTexture, interpTexcoord).r + 0.3;

  vec3 normNormal = normalize(interpNormal);
  vec3 viewDir = normalize(interpPosition);
  vec3 halfVec = normalize(dirToLight - viewDir);

  float cosIncidence = clamp(dot(normNormal, dirToLight), 0., 1.);
  float spec = pow(clamp(dot(normNormal, halfVec), 0., 1.), 32.) * specIntensity;
  float specTerm = (cosIncidence != 0.) ? spec : 0.;
  float diffuseTerm = texIntensity * (lightIntensity * cosIncidence + ambIntensity);

  outputColor = diffuseMaterial.rgb * diffuseTerm + specTerm;
}
