uniform sampler2D starTex;
varying vec4 color;

void main(void)
{
    gl_FragColor = texture2D(starTex, gl_PointCoord.xy) * color;
}
