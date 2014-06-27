float3 yuv2rgb(float y, float u, float v)
{
    y = 1.1643 * (y - 0.0625);
    u = u - 0.5;
    v = v - 0.5;

    return float3(y + 1.5958 * v, y - 0.39173 * u - 0.81290 * v, y + 2.017 * u);
}