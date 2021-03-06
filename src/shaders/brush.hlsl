
// ----------------------------------------------------------------------------

#define kMinLightDist   0.01
#define kMinLightDistSq 0.001
#define kMinAlpha       0.00001525878
#define kPi             3.141592653
#define kTau            6.283185307
#define kEpsilon        2.38418579e-7

float dotsat(float3 a, float3 b)
{
    return saturate(dot(a, b));
}

float BrdfAlpha(float roughness)
{
    return max(roughness * roughness, kMinAlpha);
}

float3 F_0(float3 albedo, float metallic)
{
    return lerp(0.04, albedo, metallic);
}

float F_90(float3 F0)
{
    return saturate(50.0f * dot(F0, 0.33));
}

float3 F_Schlick(float3 f0, float3 f90, float cosTheta)
{
    float t = 1.0 - cosTheta;
    float t5 = t * t * t * t * t;
    return lerp(f0, f90, t5);
}

float F_Schlick1(float f0, float f90, float cosTheta)
{
    float t = 1.0 - cosTheta;
    float t5 = t * t * t * t * t;
    return lerp(f0, f90, t5);
}

float3 F_SchlickEx(float3 albedo, float metallic, float cosTheta)
{
    float3 f0 = F_0(albedo, metallic);
    float f90 = F_90(f0);
    return F_Schlick(f0, f90, cosTheta);
}

float3 DiffuseColor(float3 albedo, float metallic)
{
    return albedo * (1.0 - metallic);
}

float D_GTR(float NoH, float alpha)
{
    float a2 = alpha * alpha;
    float f = lerp(1.0f, a2, NoH * NoH);
    return a2 / max(kEpsilon, f * f * kPi);
}

float G_SmithGGX(float NoL, float NoV, float alpha)
{
    float a2 = alpha * alpha;
    float v = NoL * sqrt(a2 + (NoV - NoV * a2) * NoV);
    float l = NoV * sqrt(a2 + (NoL - NoL * a2) * NoL);
    return 0.5f / max(kEpsilon, v + l);
}

float Fd_Lambert()
{
    return 1.0f / kPi;
}

float Fd_Burley(
    float NoL,
    float NoV,
    float LoH,
    float roughness)
{
    float fd90 = 0.5f + 2.0f * LoH * LoH * roughness;
    float lightScatter = F_Schlick1(1.0f, fd90, NoL);
    float viewScatter = F_Schlick1(1.0f, fd90, NoV);
    return (lightScatter * viewScatter) / kPi;
}

float3 DirectBRDF(
    float3 V,
    float3 L,
    float3 N,
    float3 albedo,
    float roughness,
    float metallic)
{
    float3 H = normalize(V + L);
    float NoV = dotsat(N, V);
    float NoH = dotsat(N, H);
    float NoL = dotsat(N, L);
    float HoV = dotsat(H, V);
    float LoH = dotsat(L, H);

    float alpha = BrdfAlpha(roughness);
    float3 F = F_SchlickEx(albedo, metallic, HoV);
    float G = G_SmithGGX(NoL, NoV, alpha);
    float D = D_GTR(NoH, alpha);
    float3 Fr = D * G * F;

    float3 Fd = DiffuseColor(albedo, metallic) * Fd_Burley(NoL, NoV, LoH, roughness);

    const float amtSpecular = 1.0;
    float amtDiffuse = 1.0 - metallic;
    float scale = 1.0 / (amtSpecular + amtDiffuse);

    return (Fr + Fd) * scale;
}

// ----------------------------------------------------------------------------

float3 UnpackEmission(float3 albedo, float e)
{
    e = e * e * e * 100.0;
    return albedo * e;
}

// http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 TonemapUncharted2(float3 x)
{
    const float a = 0.15;
    const float b = 0.50;
    const float c = 0.10;
    const float d = 0.20;
    const float e = 0.02;
    const float f = 0.30;
    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 TonemapACES(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

float3 LinearTosRGB(float3 x)
{
    float3 s1 = sqrt(x);
    float3 s2 = sqrt(s1);
    float3 s3 = sqrt(s2);
    return s1 * 0.658444 + s2 * 0.643378 + s3 * -0.298148;
}

// ----------------------------------------------------------------------------

float3x3 NormalToTBN(float3 N)
{
    const float3 kX = { 1.0, 0.0, 0.0 };
    const float3 kZ = { 0.0, 0.0, 1.0 };

    float3 a = abs(N.z) < 0.9 ? kZ : kX;
    float3 T = normalize(cross(a, N));
    float3 B = cross(N, T);

    return float3x3(T, B, N);
}

float3 TbnToWorld(float3x3 TBN, float3 nTS)
{
    float3 r = TBN[0] * nTS.x;
    float3 u = TBN[1] * nTS.y;
    float3 f = TBN[2] * nTS.z;
    float3 dir = r + u + f;
    return dir;
}

float3 TanToWorld(float3 normalWS, float3 normalTS)
{
    float3x3 TBN = NormalToTBN(normalWS);
    return TbnToWorld(TBN, normalTS);
}

// ----------------------------------------------------------------------------

struct PerDraw
{
    float4x4 localToWorld;
    float4x4 worldToLocal;
    float4 textureScale;
    float4 textureBias;
};

struct PerCamera
{
    float4x4 worldToCamera;
    float4x4 cameraToClip;
    float4 eye;
    float4 lightDir;
    float4 lightColor;
};

[[vk::push_constant]]
cbuffer push_constants
{
    uint kDrawIndex;
    uint kCameraIndex;
    uint kAlbedoIndex;
    uint kRomeIndex;
    uint kNormalIndex;
};

// "One-Set Design"
// https://gpuopen.com/wp-content/uploads/2016/03/VulkanFastPaths.pdf#page=10

// binding 0 set 0
[[vk::binding(0)]]
StructuredBuffer<PerDraw> drawData;

// binding 1 set 0
[[vk::binding(1)]]
StructuredBuffer<PerCamera> cameraData;

[[vk::binding(2)]]
SamplerState samplers[];
[[vk::binding(2)]]
Texture2D textures[];

struct VSInput
{
    float4 positionOS : POSITION;
    float4 normalOS : NORMAL;
    float4 uv01 : TEXCOORD0;
};

struct PSInput
{
    float4 positionCS : SV_Position;
    float3 positionWS : TEXCOORD0;
    float4 uv01 : TEXCOORD1;
    float3x3 TBN : TEXCOORD2;
};

float4 SampleTexture(uint index, float2 uv)
{
    return textures[index].Sample(samplers[index], uv);
}

PSInput VSMain(VSInput input)
{
    PerDraw perDraw = drawData[kDrawIndex];
    PerCamera perCamera = cameraData[kCameraIndex];
    float4 positionOS = float4(input.positionOS.xyz, 1.0);
    float4 positionWS = mul(perDraw.localToWorld, positionOS);
    float4 positionCS = mul(perCamera.cameraToClip, mul(perCamera.worldToCamera, positionWS));
    float3x3 IM = float3x3(
        perDraw.worldToLocal[0].xyz,
        perDraw.worldToLocal[1].xyz,
        perDraw.worldToLocal[2].xyz);
    float3 normalWS = mul(IM, input.normalOS.xyz);
    float3x3 TBN = NormalToTBN(normalWS);

    PSInput output;
    output.positionCS = positionCS;
    output.positionWS = positionWS.xyz;
    output.TBN = TBN;
    output.uv01 = input.uv01 * perDraw.textureScale + perDraw.textureBias;
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    uint ci = kCameraIndex;
    uint ai = kAlbedoIndex;
    uint ri = kRomeIndex;
    uint ni = kNormalIndex;
    float2 uv0 = input.uv01.xy;

    float3 albedo = SampleTexture(ai, uv0).xyz;
    float4 rome = SampleTexture(ri, uv0);
    float3 normalTS = SampleTexture(ni, uv0).xyz;

    normalTS = normalize(normalTS * 2.0 - 1.0);
    float3 N = TbnToWorld(input.TBN, normalTS);
    N = normalize(N);

    PerCamera perCamera = cameraData[ci];
    float3 P = input.positionWS;
    float3 V = normalize(perCamera.eye.xyz - P);
    float3 L = perCamera.lightDir.xyz;
    float3 lightColor = perCamera.lightColor.xyz;
    float roughness = rome.x;
    float metallic = rome.z;

    float3 brdf = DirectBRDF(V, L, N, albedo, roughness, metallic);
    float3 direct = brdf * lightColor * dotsat(N, L);
    float3 ambient = albedo * lightColor * 0.01;
    float3 emission = UnpackEmission(albedo, rome.w);
    float3 color = direct + ambient + emission;
    color = TonemapACES(color);
    color = saturate(color);
    return float4(color, 1.0);
}
