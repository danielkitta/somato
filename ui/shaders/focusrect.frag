#version 150

uniform vec4 focusColor;

out vec4 outputColor;

void main()
{
  float coord = dot(gl_FragCoord.xy, vec2(0.5));
  float alpha = step(0.5, fract(coord + 0.25));

  outputColor = focusColor * alpha;

  if (alpha < 0.5)
    discard;
}
