#version 150

uniform sampler2D pieceTexture;
uniform vec4 diffuseMaterial;

const vec3  dirToLight        = vec3(0.0, 0.242535625, 0.9701425);
const float lightIntensity    = 0.8;
const float ambientIntensity  = 0.2;
const float specularIntensity = 0.1;

smooth in vec3 interpPosition;
smooth in vec3 interpNormal;
smooth in vec2 interpTexcoord;

out vec3 outputColor;

void main()
{
  vec3 normNormal = normalize(interpNormal);
  float cosAngIncidence = clamp(dot(normNormal, dirToLight), 0.0, 1.0);

  vec3 viewDir = normalize(interpPosition);
  vec3 halfVec = normalize(dirToLight - viewDir);

  float blinnTerm = pow(clamp(dot(normNormal, halfVec), 0.0, 1.0), 32);
  float specularReflection = (cosAngIncidence != 0.0) ? blinnTerm : 0.0;

  float texIntensity = 0.5 * texture(pieceTexture, interpTexcoord).r + 0.3;
  float diffuseTerm = texIntensity * (lightIntensity * cosAngIncidence + ambientIntensity);

  outputColor = diffuseMaterial.rgb * diffuseTerm + specularIntensity * specularReflection;
}
