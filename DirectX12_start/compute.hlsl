RWStructuredBuffer<uint> g_value : register(cs, u0);

[numthreads(4,4,1)]
void CS(
    uint3 id : SV_DispatchThreadID
)
{
    g_value[0] = 1;
}