#version 150

uniform vec4 focusColor;

out vec4 outputColor;

void main()
{
  float coord = dot(gl_FragCoord, vec4(0.5, 0.5, 0.0, 0.25));
  float alpha = step(0.5, fract(coord));

  outputColor = focusColor * alpha;

  if (alpha < 0.5)
    discard;
}
