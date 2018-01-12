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

void main()
{
  float luminance = texture(pieceTexture, varTexcoord).r;

  float rmagNorm = inversesqrt(dot(varNormal, varNormal));
  float rmagHalf = inversesqrt(dot(varHalfVec, varHalfVec));

  float cosLight = clamp(dot(varNormal, dirToLight) * rmagNorm, 0., 1.);
  float cosHalf  = clamp(dot(varNormal, varHalfVec) * rmagNorm * rmagHalf, 0., 1.);

  float specular = pow(cosHalf, shininess) * specIntensity * cosLight;
  float diffuse  = luminance * (lightIntensity * cosLight + ambIntensity);

  outputColor = vec4(diffuseColor.rgb * diffuse + specular, 1.);
}
