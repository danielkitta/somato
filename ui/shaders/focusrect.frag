uniform vec4 focusColor;

out vec4 outputColor;

const float stippleStep = 1. / 3.;

void main()
{
  float coord = dot(vec3(gl_FragCoord.xy, 0.5), vec3(stippleStep));
  float alpha = step(stippleStep, fract(coord));

  outputColor = focusColor * alpha;

  if (alpha < 0.5)
    discard;
}
