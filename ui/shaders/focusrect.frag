uniform vec4 focusColor;

out vec4 outputColor;

const float stippleStep = 1. / 3.;
const float epsilon     = 1. / 1024.;

void main()
{
  float coord = dot(gl_FragCoord.xy, vec2(stippleStep));
  float alpha = round(fract(coord - epsilon));

  outputColor = focusColor * alpha;

  if (alpha < 0.5)
    discard;
}
