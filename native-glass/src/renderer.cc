#include "renderer.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>

#include "session.h"
#include "stats.h"

namespace {

constexpr float kBlurMarginCss = 40.0f;

const char kShaderSource[] = R"hlsl(
cbuffer P : register(b0) {
  float2 srcOrigin; float2 srcSpan;
  float2 outSize;   float2 dir;
  float2 glassSize; float radius; float bezel;
  float maxBend; float dispFactor; float aberration; float saturation;
  float margin; float alpha; float hdrSource; float whiteScale;
  float weatherMode; float weatherTime; float weatherIntensity; float weatherDpr;
  float weatherScale; float weatherFog; float weatherRefract; float weatherPad;
};

Texture2D tex : register(t0);
Texture2D sharpTex : register(t1);
SamplerState smp : register(s0);

float3 scRGBToDisplay(float3 color) {
  float3 v = saturate(color * whiteScale);
  return lerp(12.92 * v, 1.055 * pow(v, 1.0 / 2.4) - 0.055, step(0.0031308, v));
}

// Fullscreen triangle
float4 VSMain(uint id : SV_VertexID) : SV_Position {
  float2 pos = float2((id << 1) & 2, id & 2);
  return float4(pos * float2(2, -2) + float2(-1, 1), 0, 1);
}

// Downsample + single-direction Gaussian
float4 PSBlur(float4 pos : SV_Position) : SV_Target {
  float2 t = pos.xy / outSize;
  float2 uv = srcOrigin + t * srcSpan;
  float4 c = tex.Sample(smp, uv) * 0.4026
    + (tex.Sample(smp, uv + dir) + tex.Sample(smp, uv - dir)) * 0.2442
    + (tex.Sample(smp, uv + dir * 2.0) + tex.Sample(smp, uv - dir * 2.0)) * 0.0545;
  if (hdrSource > 0.5) {
    // scRGB is linear with SDR white boosted by the monitor's Windows white level.
    c.rgb = scRGBToDisplay(c.rgb);
    c.a = 1.0;
  }
  return c;
}

float sdRoundRect(float2 p, float2 halfSize, float r) {
  float2 q = abs(p) - halfSize + r;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float GlassRimProfile(float t) {
  return pow(1.0 - saturate(t), 2.6);
}

// A thick inward-facing rim; the flat center never displaces the backdrop.
float2 GlassRimOffset(float2 p, float2 halfSize, float r, float depth, float bezel, float maxBend) {
  if (depth < 0.0 || depth >= bezel) return float2(0, 0);
  float2 q = abs(p) - halfSize + r;
  float2 direction = max(q, 0.0);
  float len = length(direction);
  direction = len > 1e-4 ? direction / len : (q.x > q.y ? float2(1, 0) : float2(0, 1));
  direction = direction * (p / max(abs(p), 1e-4));
  return -direction * (maxBend * GlassRimProfile(depth / bezel));
}

float2 toBlurUV(float2 px) {
  return (px + margin) / (glassSize + margin * 2.0);
}

// --- Procedural Weather Effects (Apple Weather / Heartfelt inspired) ---

float W_Hash12(float2 p) {
  float3 p3 = frac(float3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return frac((p3.x + p3.y) * p3.z);
}

float3 W_Hash32(float2 p) {
  float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
  p3 += dot(p3, p3.yxz + 33.33);
  return frac((p3.xxy + p3.yzz) * p3.zyx);
}

// --- rocksdanister/weather (Lively Weather, MIT) Rain.cs HLSL Port ---
// Ported from Drizzle.UI.Shaders.D2D1.Rain (inspired by BigWIngs "Heartfelt")

float W_S(float a, float b, float t) {
  return smoothstep(a, b, t);
}

float3 W_N13(float p) {
  float3 p3 = frac(float3(p, p, p) * float3(0.1031, 0.11369, 0.13787));
  p3 += dot(p3, p3.yzx + 19.19);
  return frac(float3((p3.x + p3.y) * p3.z, (p3.x + p3.z) * p3.y, (p3.y + p3.z) * p3.x));
}

float W_N1(float t) {
  return frac(sin(t * 12345.564) * 7658.76);
}

float W_Saw(float b, float t) {
  return W_S(0.0, b, t) * W_S(1.0, b, t);
}

float2 W_DropLayer2(float2 uv, float t) {
  float2 UV = uv;

  uv.y += t * 0.75;
  float2 a = float2(6.0, 1.0);
  float2 grid = a * 2.0;
  float2 id = floor(uv * grid);

  float colShift = W_N1(id.x);
  uv.y += colShift;

  id = floor(uv * grid);
  float3 n = W_N13(id.x * 35.2 + id.y * 2376.1);
  float2 st = frac(uv * grid) - float2(0.5, 0.0);

  float x = n.x - 0.5;

  float y = UV.y * 20.0;
  float wiggle = sin(y + sin(y));
  x += wiggle * (0.5 - abs(x)) * (n.z - 0.5);
  x *= 0.7;
  float ti = frac(t + n.z);
  y = (W_Saw(0.85, ti) - 0.5) * 0.9 + 0.5;
  float2 p = float2(x, y);

  float d = length((st - p) * a.yx);

  float mainDrop = W_S(0.28, 0.0, d);

  float r = sqrt(W_S(1.0, y, st.y));
  float cd = abs(st.x - x);
  float trail = W_S(0.16 * r, 0.10 * r * r, cd);
  float trailFront = W_S(-0.02, 0.02, st.y - y);
  trail *= trailFront * r * r;

  y = UV.y;
  float trail2 = W_S(0.14 * r, 0.0, cd);
  float droplets = max(0.0, (sin(y * (1.0 - y) * 120.0) - st.y)) * trail2 * trailFront * n.z;
  y = frac(y * 12.0) + (st.y - 0.5);
  float dd = length(st - float2(x, y));
  droplets = W_S(0.17, 0.0, dd);
  float trailBead = droplets * r * trailFront * 0.86;
  float h = saturate(0.5 + 0.5 * (trailBead - mainDrop) / 0.06);
  float m = lerp(mainDrop, trailBead, h) + 0.06 * h * (1.0 - h);

  return float2(m, trail);
}

float W_StaticDropsOrig(float2 uv, float t) {
  // Island-scaled clean static droplets (avoids 1px noise speckles while naturally balancing large sliding drops)
  uv *= 12.0;

  float2 id = floor(uv);
  uv = frac(uv) - 0.5;
  float3 n = W_N13(id.x * 107.45 + id.y * 3543.0);
  if (n.z > 0.34) return 0.0;
  float2 p = (n.xy - 0.5) * 0.56;
  float d = length(uv - p);

  float r = lerp(0.18, 0.30, frac(n.z * 17.3));
  float fade = W_Saw(0.04, frac(t + n.z));
  return W_S(r, r * 0.12, d) * (0.72 + 0.28 * frac(n.x * 10.0)) * fade;
}

float2 W_DropsOrig(float2 uv, float t, float l0, float l1, float l2) {
  float s = l0 > 0.001 ? W_StaticDropsOrig(uv, t) * l0 : 0.0;
  float2 m1 = W_DropLayer2(uv, t) * l1;
  float2 m2 = W_DropLayer2(uv * 1.65, t) * l2;

  float c = s + m1.x + m2.x;
  // Tighter edge transition brings droplets into crisp macro focus instead of soft/blurry edges
  c = W_S(0.32, 0.80, c);

  return float2(c, max(m1.y * l1, m2.y * l2));
}

// Value noise + 4-octave FBM for clouds/fog
float W_Noise(float2 p) {
  float2 i = floor(p);
  float2 f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = W_Hash12(i);
  float b = W_Hash12(i + float2(1.0, 0.0));
  float c = W_Hash12(i + float2(0.0, 1.0));
  float d = W_Hash12(i + float2(1.0, 1.0));
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float W_Fbm(float2 p) {
  float v = 0.0;
  float a = 0.5;
  float2x2 rot = float2x2(0.8, 0.6, -0.6, 0.8);
  for (int i = 0; i < 4; ++i) {
    v += a * W_Noise(p);
    p = mul(rot, p) * 2.02 + float2(17.1, 31.7);
    a *= 0.5;
  }
  return v;
}

// Procedural jagged lightning bolt with main trunk + 2 forks (returns x = hot core, y = electric halo)
float2 W_LightningBolt(float2 uv, float seed, float progress) {
  float startX = lerp(0.24, 0.84, W_Hash12(float2(seed, 1.7)));
  float drift = lerp(-0.14, 0.14, W_Hash12(float2(seed, 4.3)));

  // Vertical strike growth from top (0) to bottom (1)
  float reach = smoothstep(progress + 0.04, progress - 0.12, uv.y);
  if (reach <= 0.001) return float2(0.0, 0.0);

  // Multi-octave jagged horizontal displacement along y
  float j1 = (W_Noise(float2(uv.y * 5.5, seed * 3.1)) - 0.5) * 0.16;
  float j2 = (W_Noise(float2(uv.y * 15.0, seed * 7.7 + 2.3)) - 0.5) * 0.055;
  float j3 = (W_Noise(float2(uv.y * 36.0, seed * 13.1 + 5.1)) - 0.5) * 0.018;
  float trunkX = startX + drift * uv.y + (j1 + j2 + j3) * smoothstep(0.0, 0.12, uv.y);

  float dMain = abs(uv.x - trunkX);
  float taper = lerp(1.15, 0.55, uv.y);
  float core = (0.0022 * taper) / (dMain + 0.0016);
  float halo = (0.018 * taper) / (dMain + 0.016);

  // Branch 1 (forks off around upper-middle)
  float b1Start = lerp(0.18, 0.36, W_Hash12(float2(seed, 9.1)));
  float b1Dir = W_Hash12(float2(seed, 11.4)) > 0.5 ? 1.0 : -1.0;
  float b1t = uv.y - b1Start;
  if (b1t > 0.0 && b1t < 0.42) {
    float b1Fade = smoothstep(0.42, 0.0, b1t) * smoothstep(0.0, 0.03, b1t);
    float b1J = (W_Noise(float2(uv.y * 18.0, seed * 5.9 + 9.0)) - 0.5) * 0.045;
    float b1StartX = startX + drift * b1Start + ((W_Noise(float2(b1Start * 5.5, seed * 3.1)) - 0.5) * 0.16
                   + (W_Noise(float2(b1Start * 15.0, seed * 7.7 + 2.3)) - 0.5) * 0.055) * smoothstep(0.0, 0.12, b1Start);
    float b1X = b1StartX + b1Dir * b1t * 0.48 + b1J;
    float dB1 = abs(uv.x - b1X);
    core += ((0.0011 * b1Fade) / (dB1 + 0.0015));
    halo += ((0.0085 * b1Fade) / (dB1 + 0.014));
  }

  // Branch 2 (smaller lower fork on opposite side)
  float b2Start = lerp(0.42, 0.62, W_Hash12(float2(seed, 15.8)));
  float b2Dir = -b1Dir;
  float b2t = uv.y - b2Start;
  if (b2t > 0.0 && b2t < 0.32) {
    float b2Fade = smoothstep(0.32, 0.0, b2t) * smoothstep(0.0, 0.025, b2t);
    float b2J = (W_Noise(float2(uv.y * 22.0, seed * 8.3 + 4.0)) - 0.5) * 0.035;
    float b2StartX = startX + drift * b2Start + ((W_Noise(float2(b2Start * 5.5, seed * 3.1)) - 0.5) * 0.16
                   + (W_Noise(float2(b2Start * 15.0, seed * 7.7 + 2.3)) - 0.5) * 0.055) * smoothstep(0.0, 0.12, b2Start);
    float b2X = b2StartX + b2Dir * b2t * 0.42 + b2J;
    float dB2 = abs(uv.x - b2X);
    core += ((0.00085 * b2Fade) / (dB2 + 0.0015));
    halo += ((0.0065 * b2Fade) / (dB2 + 0.013));
  }

  return float2(saturate(core) * reach, saturate(halo) * reach);
}

// Domain-warped drifting cloud/mist field (returns x = density, y = top-lit highlight)
float2 W_CloudField(float2 uv, float t) {
  float2 p1 = uv * 0.78 + float2(t * 0.085, -t * 0.018);
  float2 warp = float2(W_Fbm(p1 + float2(1.3, 4.7)), W_Fbm(p1 + float2(6.2, 2.1)));
  float baseCloud = W_Fbm(p1 + (warp - 0.5) * 1.15);
  float2 p2 = uv * 1.45 + float2(t * 0.14, t * 0.025) + (warp - 0.5) * 0.65;
  float wisp = W_Fbm(p2);
  float density = smoothstep(0.28, 0.78, baseCloud * 0.68 + wisp * 0.36);
  // Subtle directional top-left lighting on cloud ridges
  float litSample = W_Fbm(p1 + float2(-0.08, -0.10) + (warp - 0.5) * 1.15);
  float light = saturate((baseCloud - litSample) * 2.8 + 0.52);
  return float2(density, light);
}

// Seamless 3x3-neighborhood astronomical starfield: pinpoint realistic stars, zero square cell clipping
float3 W_StarLayer(float2 uv, float t, float scale, float prob, float maxSize, float brightness) {
  float2 st = uv * scale;
  float2 baseId = floor(st);
  float2 baseF = frac(st);
  float3 acc = float3(0.0, 0.0, 0.0);

  for (int oy = -1; oy <= 1; ++oy) {
    for (int ox = -1; ox <= 1; ++ox) {
      float2 cell = baseId + float2(ox, oy);
      float3 n = W_Hash32(cell);
      if (n.z < (1.0 - prob)) continue;

      float2 starPos = float2(ox, oy) + 0.18 + n.xy * 0.64;
      float2 d = baseF - starPos;
      float dist = length(d);

      // Pinpoint high-DPI star core + very delicate soft micro-glow (no tacky giant cross stickers)
      float mag = frac(n.z * 37.3);
      float sz = lerp(maxSize * 0.45, maxSize, mag * mag);
      float core = exp(-(dist * dist) / max(sz * sz * 0.35, 1e-6));
      float halo = exp(-dist / max(sz * 2.2, 1e-4)) * (0.14 + 0.18 * mag);

      // Slow organic atmospheric scintillation (gentle twinkle, not harsh strobing)
      float twinkle = 0.68 + 0.32 * sin(t * (0.9 + n.x * 2.2) + n.y * 41.0);
      float lum = (core + halo) * twinkle * brightness * (0.45 + 0.55 * mag);

      // Authentic subtle astronomical starlight tints (ice blue, crisp white, warm champagne)
      float temp = frac(n.x * 13.7 + n.y * 7.1);
      float3 col = temp < 0.36 ? float3(0.78, 0.88, 1.0)
                 : (temp < 0.78 ? float3(0.97, 0.98, 1.0)
                                : float3(1.0, 0.92, 0.80));
      acc += col * lum;
    }
  }
  return acc;
}

// Occasional ultra-fine diagonal shooting star gliding smoothly across the night glass
float3 W_ShootingStar(float2 uv, float t) {
  float rate = 0.16; // one subtle meteor every ~6s
  float cycle = floor(t * rate);
  float phase = frac(t * rate);
  float active = step(0.35, W_Hash12(float2(cycle, 41.9)));
  if (active < 0.5 || phase > 0.18) return float3(0.0, 0.0, 0.0);

  float prog = phase / 0.18;
  float2 startP = float2(lerp(0.30, 0.90, W_Hash12(float2(cycle, 7.3))),
                         lerp(0.08, 0.36, W_Hash12(float2(cycle, 13.7))));
  float2 dir = normalize(float2(-0.90, 0.43));
  float travel = 0.46;
  float2 head = startP + dir * (prog * travel);
  float2 rel = uv - head;
  float along = dot(rel, -dir);
  float perp = abs(dot(rel, float2(-dir.y, dir.x)));

  float tailLen = 0.15 * sin(prog * 3.14159265);
  float tailMask = smoothstep(tailLen, 0.0, along) * step(0.0, along);
  float headGlow = exp(-length(rel) * 110.0);
  float streak = exp(-perp * 420.0) * tailMask * 0.85 + exp(-perp * 140.0) * tailMask * 0.18;
  float fade = sin(prog * 3.14159265);
  return float3(0.90, 0.95, 1.0) * (streak + headGlow) * fade;
}

// Sparse floating sunlit dust motes drifting gently inside warm sunbeams
float W_SunMotes(float2 uv, float t) {
  float motes = 0.0;
  float2 st1 = uv * 3.6 + float2(t * 0.045, t * 0.085);
  float2 id1 = floor(st1);
  float2 f1 = frac(st1) - 0.5;
  float3 n1 = W_Hash32(id1 + float2(13.7, 5.2));
  if (n1.z > 0.74) {
    float2 c1 = (n1.xy - 0.5) * 0.46 + float2(sin(t * 0.7 + n1.x * 6.28), cos(t * 0.55 + n1.y * 6.28)) * 0.10;
    float r1 = lerp(0.024, 0.052, frac(n1.z * 9.3));
    float pulse1 = 0.45 + 0.55 * sin(t * (1.3 + n1.x * 1.6) + n1.y * 19.0);
    motes += smoothstep(r1 + 0.035, r1 * 0.15, length(f1 - c1)) * pulse1 * 0.72;
  }
  float2 st2 = uv * 6.4 + float2(-t * 0.03, t * 0.12);
  float2 id2 = floor(st2);
  float2 f2 = frac(st2) - 0.5;
  float3 n2 = W_Hash32(id2 + float2(31.1, 17.9));
  if (n2.z > 0.80) {
    float2 c2 = (n2.xy - 0.5) * 0.48 + float2(cos(t * 0.9 + n2.y * 6.28), sin(t * 0.8 + n2.x * 6.28)) * 0.08;
    float r2 = lerp(0.018, 0.038, frac(n2.x * 11.7));
    float pulse2 = 0.35 + 0.65 * sin(t * (1.8 + n2.y * 2.0) + n2.x * 23.0);
    motes += smoothstep(r2 + 0.025, r2 * 0.15, length(f2 - c2)) * pulse2 * 0.52;
  }
  return saturate(motes);
}

// Multi-depth falling snow layer (sparse, clean soft-bokeh flakes without cell clipping)
float W_SnowLayer(float2 uv, float t, float scale, float speed, float wind, float softness, float prob) {
  float2 st = uv * scale;
  st.y -= t * speed;
  st.x += sin(t * 0.7 + st.y * 0.6) * wind;
  float2 id = floor(st);
  float2 f = frac(st) - 0.5;
  float3 n = W_Hash32(id);
  if (n.z > prob) return 0.0;
  float2 offset = (n.xy - 0.5) * 0.42;
  offset.x += sin(t * (1.1 + n.z * 1.5) + n.x * 6.283) * 0.09;
  float d = length(f - offset);
  float r = lerp(0.045, 0.105, frac(n.z * 7.3));
  float flake = smoothstep(r + softness, r * 0.25, d);
  return flake * (0.45 + 0.55 * frac(n.y * 5.7));
}

// Main lens shading: SDF displacement + RGB dispersion + saturation + live weather glass FX
float4 PSLens(float4 pos : SV_Position) : SV_Target {
  float2 css = pos.xy;
  float2 halfSize = glassSize * 0.5;
  float2 p = css - halfSize;
  float r = min(radius, min(halfSize.x, halfSize.y));

  float sd = sdRoundRect(p, halfSize, r);
  float coverage = saturate(0.5 - sd);
  if (coverage <= 0.0) return float4(0, 0, 0, 0);

  float depth = -sd;
  float2 disp = GlassRimOffset(p, halfSize, r, depth, bezel, maxBend) * min(dispFactor, 1.0);
  float2 nrm = -disp / max(length(disp), 1e-4);
  float edgeW = pow(1.0 - saturate(depth / bezel), 2.0);
  float2 lensCss = css;

  float wMode = weatherMode;
  float wTime = weatherTime;
  float wInt = clamp(weatherIntensity, 0.0, 1.5);
  float wScale = clamp(weatherScale, 0.4, 2.5);
  float wFog = saturate(weatherFog);
  float wRefract = clamp(weatherRefract, 0.0, 2.2);
  float dpr = max(weatherDpr, 0.5);

  // Physical-pixel anchored coordinate (prevents stretching between collapsed pill and expanded card)
  float2 physUV = p / (96.0 * dpr * wScale);

  float rainDrop = 0.0;
  float rainTrail = 0.0;
  float2 rainNormal = float2(0.0, 0.0);
  float lightning = 0.0;
  float stormCycle = 0.0;
  float stormProgress = 0.0;
  float2 cloudSample = float2(0.0, 0.0);

  // Mode 1: Rain | Mode 2: Thunderstorm — Plan 2: original rocksdanister/weather DropLayer2
  // adapted to island scale (static micro-noise disabled; only clean meandering sliding drops refract backdrop)
  float3 origRainCol = float3(0.0, 0.0, 0.0);
  bool useOrigRain = (wMode > 0.5 && wMode < 2.5);
  if (useOrigRain) {
    float stormBoost = wMode > 1.5 ? 1.0 : 0.0;
    float refH = 260.0 * dpr * wScale;
    float2 rUV = float2(p.x, -p.y) / refH;
    rUV *= 0.85; // scaled for 170px island so 4-6 clean meandering drops glide smoothly across
    float rt = wTime * 0.2 * (0.55 + 0.22 * stormBoost);
    float rainAmount = saturate((0.68 + 0.24 * stormBoost) * wInt);

    // Balanced island-scaled static droplets (clean 4-6px beads, no 1px noise) alongside meandering drops
    float staticDrops = W_S(0.0, 0.85, rainAmount) * 0.85;
    float layer1 = W_S(0.15, 0.70, rainAmount);
    float layer2 = W_S(0.10, 0.65, rainAmount) * 0.65;

    float2 c0 = W_DropsOrig(rUV, rt, staticDrops, layer1, layer2);
    // Finer normal step for sharper, higher-definition droplet edges
    float2 e = float2(0.00075, 0.0) * (0.5 * wRefract);
    float cx = W_DropsOrig(rUV + e, rt, staticDrops, layer1, layer2).x;
    float cy = W_DropsOrig(rUV + e.yx, rt, staticDrops, layer1, layer2).x;
    float2 n = float2(cx - c0.x, cy - c0.x);

    // Pure neutral crisp optical refraction: keep droplet RGB dispersion minimal so refracted detail stays razor-sharp
    float2 refractPx = float2(n.x, -n.y) * (refH * 1.25);
    float2 samplePx = css + disp + refractPx;
    float2 rainChroma = disp * aberration + refractPx * (aberration * 0.16);
    origRainCol = float3(
        sharpTex.Sample(smp, toBlurUV(samplePx + rainChroma)).r,
        sharpTex.Sample(smp, toBlurUV(samplePx)).g,
        sharpTex.Sample(smp, toBlurUV(samplePx - rainChroma)).b);
    if (hdrSource > 0.5) origRainCol = scRGBToDisplay(origRainCol);

    // Subtle crisp micro-specular glint on sharp top-left meniscus (adds focus clarity without fake painted blobs)
    float2 slope = float2(n.x, -n.y);
    float slopeMag = length(slope);
    if (slopeMag > 1e-4) {
      float2 sDir = slope / slopeMag;
      float glint = pow(saturate(dot(sDir, normalize(float2(-0.55, -0.83)))), 6.0) * saturate(slopeMag * 36.0);
      origRainCol = saturate(origRainCol + float3(0.96, 0.98, 1.0) * glint * 0.22);
    }

    if (stormBoost > 0.5) {
      // Preserved jagged downward lightning bolt strike
      float rate = 0.28;
      stormCycle = floor(wTime * rate);
      float phase = frac(wTime * rate);
      float trigger = step(0.22, W_Hash12(float2(stormCycle, 19.37)));
      stormProgress = saturate((phase - 0.06) / 0.075);
      float f1 = smoothstep(0.055, 0.095, phase) * exp(-max(phase - 0.095, 0.0) * 18.0);
      float f2 = smoothstep(0.18, 0.205, phase) * exp(-max(phase - 0.205, 0.0) * 24.0) * 0.72;
      lightning = trigger * saturate(f1 + f2) * min(wInt, 1.3);
    }
  } else if (wMode > 3.5 && wMode < 4.5) {
    cloudSample = W_CloudField(physUV, wTime);
  }

  float2 chroma = disp * aberration;
  float3 c = float3(
      tex.Sample(smp, toBlurUV(lensCss + disp + chroma)).r,
      tex.Sample(smp, toBlurUV(lensCss + disp)).g,
      tex.Sample(smp, toBlurUV(lensCss + disp - chroma)).b);
  float3 sharp = float3(
      sharpTex.Sample(smp, toBlurUV(lensCss + disp + chroma)).r,
      sharpTex.Sample(smp, toBlurUV(lensCss + disp)).g,
      sharpTex.Sample(smp, toBlurUV(lensCss + disp - chroma)).b);
  if (hdrSource > 0.5) sharp = scRGBToDisplay(sharp);

  float sharpMix = 0.80 + 0.20 * (1.0 - smoothstep(0.0, 1.0, saturate(depth / bezel)));

  if (wMode > 3.5 && wMode < 4.5) {
    // Drifting mist visibly softens the glass behind thicker cloud banks while gaps stay clearer
    float mistBlur = lerp(0.86, 0.14, saturate(cloudSample.x * wInt));
    sharpMix = lerp(sharpMix, mistBlur, saturate((0.45 + 0.55 * wFog) * wInt));
  }

  c = lerp(c, sharp, sharpMix);
  if (useOrigRain) {
    // Cross-fade cleanly into pure rocksdanister/weather Rain.cs output
    c = lerp(c, origRainCol, saturate(wInt));
  }
  float lum = dot(c, float3(0.213, 0.715, 0.072));
  c = lerp(lum.xxx, c, saturation);

  // Broad rim lighting hugging the boundary (light from top-left)
  float lightDot = dot(nrm, float2(-0.40, -0.92));
  c = saturate(c + edgeW * (0.10 * saturate(lightDot) + 0.015 * saturate(-lightDot)));

  // --- Weather surface shading & atmospheric overlays ---
  if (useOrigRain) {
    // Only preserved jagged downward lightning bolt strike in Thunderstorm mode (zero extra edits to Rain.cs)
    if (lightning > 0.005) {
      float2 normUV = css / max(glassSize, 1.0);
      float2 bolt = W_LightningBolt(normUV, stormCycle, stormProgress);
      float3 coreCol = float3(0.97, 0.99, 1.0);
      float3 arcHaloCol = float3(0.58, 0.76, 1.0);
      float3 skyFlash = float3(0.68, 0.80, 1.0) * lightning * 0.20;
      c = saturate(c + skyFlash + arcHaloCol * bolt.y * lightning * 0.85 + coreCol * bolt.x * lightning * 1.15);
    }
  } else if (wMode > 2.5 && wMode < 3.5) {
    // Mode 3: Multi-depth falling snow + soft winter frost rim
    float snow = 0.0;
    snow += W_SnowLayer(physUV, wTime, 1.5, 0.35, 0.32, 0.075, 0.28) * 0.58;
    snow += W_SnowLayer(physUV + float2(4.2, 1.7), wTime, 2.5, 0.50, 0.40, 0.065, 0.26) * 0.72;
    snow += W_SnowLayer(physUV + float2(9.1, 6.3), wTime, 4.0, 0.68, 0.48, 0.050, 0.22) * 0.46;
    snow += W_SnowLayer(physUV + float2(2.7, 8.4), wTime, 6.0, 0.26, 0.22, 0.040, 0.18) * 0.28;
    snow = saturate(snow * wInt);
    float3 coldTint = float3(0.72, 0.85, 0.98);
    c = lerp(c, c * 0.90 + coldTint * 0.14, saturate(0.30 * wInt * (0.3 + 0.7 * wFog)));
    c = saturate(c + float3(0.95, 0.98, 1.0) * snow * 0.85 + coldTint * edgeW * 0.14 * min(wInt, 1.2));
  } else if (wMode > 3.5 && wMode < 4.5) {
    // Mode 4: Visible volumetric drifting mist & domain-warped cloud banks
    float2 normUV = css / max(glassSize, 1.0);
    float cloudMask = saturate(cloudSample.x * wInt);
    float cloudLight = cloudSample.y;
    // Cool overcast atmosphere base + layered silver-white drifting vapor
    float3 overcastBase = float3(0.46, 0.52, 0.60);
    c = lerp(c, c * 0.82 + overcastBase * 0.26, saturate(0.32 * wInt * (0.4 + 0.6 * wFog)));
    float3 shadowCol = float3(0.54, 0.60, 0.68);
    float3 litCol = float3(0.92, 0.95, 0.99);
    float3 cloudCol = lerp(shadowCol, litCol, cloudLight);
    // Gentle horizon mist gradient (slightly denser mist flowing along lower half)
    float lowerMist = smoothstep(0.15, 0.95, normUV.y) * 0.20;
    float mistAlpha = saturate((cloudMask * 0.52 + lowerMist * wInt) * (0.45 + 0.55 * wFog));
    c = lerp(c, cloudCol, mistAlpha);
    // Soft rim frost/mist sheen
    c = saturate(c + float3(0.84, 0.90, 0.96) * edgeW * 0.08 * min(wInt, 1.2));
  } else if (wMode > 4.5 && wMode < 5.5) {
    // Mode 5: Pure warm sunlight spilling naturally across neutral glass (zero blue cast, no sticker hexagons)
    float2 normUV = css / max(glassSize, 1.0);
    // Anchor sun in physical pixel space near top-right corner so collapsed pill & expanded card look equally natural
    float2 sunPx = float2(glassSize.x - 10.0 * dpr, -8.0 * dpr)
                 + float2(sin(wTime * 0.22) * 8.0, cos(wTime * 0.18) * 5.0) * dpr;
    float2 dPx = (css - sunPx) / (155.0 * dpr * wScale);
    float dist = length(dPx);

    // Pure warm champagne-gold sunlight diffusion (leaves the shaded side 100% true neutral glass, never blue)
    float sunGlow = exp(-dist * 1.75) * (0.93 + 0.07 * sin(wTime * 0.85));
    float warmWash = exp(-dist * 0.68) * 0.38;

    // Silky, broad diagonal sunbeams drifting smoothly like sunlight through leaves/clouds (not spiky spokes)
    float diagCoord = dot(dPx, normalize(float2(0.56, 0.83)));
    float shaft1 = exp(-pow(diagCoord + 0.18 + 0.12 * sin(wTime * 0.26), 2.0) * 6.5);
    float shaft2 = exp(-pow(diagCoord - 0.34 + 0.14 * cos(wTime * 0.21 + 1.2), 2.0) * 5.0) * 0.72;
    float shaft3 = exp(-pow(diagCoord + 0.68 + 0.10 * sin(wTime * 0.17 + 2.5), 2.0) * 4.5) * 0.48;
    float shimmer = 0.72 + 0.28 * W_Noise(float2(diagCoord * 2.5 + wTime * 0.14, dist * 0.8));
    float sunBeams = (shaft1 + shaft2 + shaft3) * shimmer * exp(-dist * 0.72) * smoothstep(0.04, 0.28, dist);

    // Warm sunlit dust motes sparkling gently inside the sunbeams
    float litRegion = saturate(exp(-dist * 0.70) * 0.65 + sunBeams * 1.15);
    float motes = W_SunMotes(physUV, wTime) * litRegion;

    // Golden sunlight kissing the top-right liquid-glass rim
    float rimWarmth = edgeW * exp(-dist * 1.25) * 0.45;

    float3 sunLight = (float3(1.0, 0.93, 0.76) * sunGlow * 0.34
                     + float3(1.0, 0.82, 0.50) * (warmWash * 0.22 + sunBeams * 0.28)
                     + float3(1.0, 0.95, 0.78) * motes * 0.55
                     + float3(1.0, 0.90, 0.66) * rimWarmth) * min(wInt, 1.3);

    // Gentle Screen blend adds realistic warm daylight glow without greying or blowing out white text
    c = saturate(1.0 - (1.0 - c) * (1.0 - saturate(sunLight)));
  } else if (wMode > 5.5 && wMode < 6.5) {
    // Mode 6: Refined Apple Astronomical Night — deep clean midnight glass, subtle Milky Way silk,
    // 3x3-neighborhood pinpoint stars (zero square clipping, zero tacky cross stickers) & fine shooting star
    float2 normUV = css / max(glassSize, 1.0);
    // Deep, crisp midnight darkening (rich ink-indigo, never washed-out grey)
    float3 zenithCol = float3(0.015, 0.028, 0.078);
    float3 horizonCol = float3(0.038, 0.062, 0.135);
    float3 nightTint = lerp(zenithCol, horizonCol, smoothstep(0.0, 1.0, normUV.y));
    float darkMix = saturate(0.68 * min(wInt, 1.0) * (0.45 + 0.55 * wFog));
    c = lerp(c, c * 0.38 + nightTint * 0.68, darkMix);

    // Ultra-subtle diagonal Milky Way stardust veil
    float bandAxis = (normUV.y - 0.50) - (normUV.x - 0.50) * -0.42;
    float bandEnv = exp(-bandAxis * bandAxis * 14.0);
    float neb1 = W_Fbm(physUV * 0.95 + float2(wTime * 0.006, -wTime * 0.003));
    float neb2 = W_Fbm(physUV * 1.90 - float2(wTime * 0.004, 2.7));
    float milky = bandEnv * smoothstep(0.36, 0.86, neb1 * 0.62 + neb2 * 0.38);
    float3 milkyCol = lerp(float3(0.10, 0.18, 0.38), float3(0.22, 0.20, 0.42), neb2);
    c = saturate(c + milkyCol * milky * 0.28 * wInt);

    // 3-depth seamless pinpoint starfield (tiny distant stardust -> crisp medium stars -> rare brighter anchor stars)
    float3 stars = float3(0.0, 0.0, 0.0);
    stars += W_StarLayer(physUV + float2(1.7, 3.2), wTime, 11.0, 0.24, 0.032, 0.48);
    stars += W_StarLayer(physUV + float2(5.4, 8.1), wTime, 5.8,  0.19, 0.036, 0.78);
    stars += W_StarLayer(physUV + float2(9.3, 2.6), wTime, 2.9,  0.14, 0.040, 1.05);

    // Subtle cool moonlit edge sheen along top-left rim + occasional ultra-fine shooting star
    float3 meteor = W_ShootingStar(normUV, wTime);
    c = saturate(c + (stars * 0.95 + meteor * 1.05) * wInt + float3(0.55, 0.70, 0.96) * edgeW * 0.07 * wInt);
  }

  float a = coverage * alpha;
  return float4(c * a, a);
}
)hlsl";

struct ShaderParams {
    float srcOrigin[2];
    float srcSpan[2];
    float outSize[2];
    float dir[2];
    float glassSize[2];
    float radius;
    float bezel;
    float maxBend;
    float dispFactor;
    float aberration;
    float saturation;
    float margin;
    float alpha;
    float hdrSource;
    float whiteScale;
    float weatherMode;
    float weatherTime;
    float weatherIntensity;
    float weatherDpr;
    float weatherScale;
    float weatherFog;
    float weatherRefract;
    float weatherPad;
};

HRESULT CompileShader(const char* entry, const char* target, ComPtr<ID3DBlob>* blob) {
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, nullptr, nullptr, nullptr,
                            entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            blob->GetAddressOf(), &errors);
    return hr;
}

}  // namespace

RECT GlassRenderer::RegionForPanel(const RECT& panelBounds, const GlassParams& params, float dpr) {
    const LONG margin = static_cast<LONG>(std::lround(kBlurMarginCss * dpr));
    const LONG offsetX = static_cast<LONG>(std::lround(params.sourceOffsetX));
    const LONG offsetY = static_cast<LONG>(std::lround(params.sourceOffsetY));
    return RECT{ panelBounds.left + offsetX - margin, panelBounds.top + offsetY - margin,
                 panelBounds.right + offsetX + margin, panelBounds.bottom + offsetY + margin };
}

HRESULT GlassRenderer::Initialize(ID3D11Device* device) {
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> blurBlob;
    ComPtr<ID3DBlob> lensBlob;
    HRESULT hr = CompileShader("VSMain", "vs_5_0", &vsBlob);
    if (FAILED(hr)) return hr;
    hr = CompileShader("PSBlur", "ps_5_0", &blurBlob);
    if (FAILED(hr)) return hr;
    hr = CompileShader("PSLens", "ps_5_0", &lensBlob);
    if (FAILED(hr)) return hr;

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_);
    if (FAILED(hr)) return hr;
    hr = device->CreatePixelShader(blurBlob->GetBufferPointer(), blurBlob->GetBufferSize(), nullptr, &blurPs_);
    if (FAILED(hr)) return hr;
    hr = device->CreatePixelShader(lensBlob->GetBufferPointer(), lensBlob->GetBufferSize(), nullptr, &lensPs_);
    if (FAILED(hr)) return hr;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device->CreateSamplerState(&sd, &sampler_);
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(ShaderParams);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return device->CreateBuffer(&bd, nullptr, &cb_);
}

HRESULT GlassRenderer::EnsureTargets(ID3D11Device* device, UINT regionW, UINT regionH,
                                     UINT panelW, UINT panelH, DXGI_FORMAT format) {
    (void)panelW;
    (void)panelH;
    if (regionTex_ && regionW == regionW_ && regionH == regionH_ && regionFormat_ == format) return S_OK;

    regionW_ = regionW;
    regionH_ = regionH;
    regionFormat_ = format;
    blurW_ = (regionW + 1) / 2;
    blurH_ = (regionH + 1) / 2;
    stagingTex_.Reset();
    stagingValid_ = false;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = regionW;
    td.Height = regionH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device->CreateTexture2D(&td, nullptr, regionTex_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    hr = device->CreateShaderResourceView(regionTex_.Get(), nullptr, regionSrv_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    td.Width = blurW_;
    td.Height = blurH_;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    hr = device->CreateTexture2D(&td, nullptr, blurTexA_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    hr = device->CreateRenderTargetView(blurTexA_.Get(), nullptr, blurRtvA_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    hr = device->CreateShaderResourceView(blurTexA_.Get(), nullptr, blurSrvA_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    hr = device->CreateTexture2D(&td, nullptr, blurTexB_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    hr = device->CreateRenderTargetView(blurTexB_.Get(), nullptr, blurRtvB_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    return device->CreateShaderResourceView(blurTexB_.Get(), nullptr, blurSrvB_.ReleaseAndGetAddressOf());
}

HRESULT GlassRenderer::RenderFromDesktop(ID3D11DeviceContext* ctx, ID3D11Texture2D* desktopTex,
                                         const RECT& desktopRect, GlassPanel& panel, float dpr) {
    ComPtr<ID3D11Device> device;
    ctx->GetDevice(&device);

    const RECT region = RegionForPanel(panel.bounds(), panel.params(), dpr);
    const UINT regionW = region.right - region.left;
    const UINT regionH = region.bottom - region.top;
    const RECT& pb = panel.bounds();
    D3D11_TEXTURE2D_DESC sourceDesc{};
    desktopTex->GetDesc(&sourceDesc);
    if (sourceDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
        sourceDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT &&
        sourceDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) return E_INVALIDARG;
    HRESULT hr = EnsureTargets(device.Get(), regionW, regionH, pb.right - pb.left, pb.bottom - pb.top, sourceDesc.Format);
    if (FAILED(hr)) return hr;

    // Region copy: desktop texture coords = virtual desktop coords - output
    // origin; out-of-bounds parts are clipped (destination offset stays aligned)
    RECT clamped{
        std::max(region.left, desktopRect.left), std::max(region.top, desktopRect.top),
        std::min(region.right, desktopRect.right), std::min(region.bottom, desktopRect.bottom)
    };
    if (clamped.right <= clamped.left || clamped.bottom <= clamped.top) return S_FALSE;

    D3D11_BOX box{};
    box.left = clamped.left - desktopRect.left;
    box.top = clamped.top - desktopRect.top;
    box.right = clamped.right - desktopRect.left;
    box.bottom = clamped.bottom - desktopRect.top;
    box.back = 1;
    ctx->CopySubresourceRegion(regionTex_.Get(), 0,
                               clamped.left - region.left, clamped.top - region.top, 0,
                               desktopTex, 0, &box);
    hasFrame_ = true;
    RunPasses(ctx, panel, dpr);
    return S_OK;
}

HRESULT GlassRenderer::Redraw(ID3D11DeviceContext* ctx, GlassPanel& panel, float dpr) {
    if (!hasFrame_) return S_FALSE;
    RunPasses(ctx, panel, dpr);
    return S_OK;
}

HRESULT GlassRenderer::EnsureStaging(ID3D11DeviceContext* ctx) {
    if (stagingTex_) return S_OK;
    ComPtr<ID3D11Device> device;
    ctx->GetDevice(&device);
    D3D11_TEXTURE2D_DESC td{};
    blurTexB_->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return device->CreateTexture2D(&td, nullptr, stagingTex_.ReleaseAndGetAddressOf());
}

HRESULT GlassRenderer::SampleBands(ID3D11DeviceContext* ctx, const std::vector<LumaBand>& bands,
                                   float dpr, std::vector<LumaBandStats>* out) {
    if (!hasFrame_ || !blurTexB_) return S_FALSE;

    // Normal path: RunPasses already queued the copy after the blur passes;
    // this is only a fallback (band config arrived after the latest render,
    // so staging has no content yet)
    if (!stagingValid_) {
        HRESULT hr = EnsureStaging(ctx);
        if (FAILED(hr)) return hr;
        ctx->CopyResource(stagingTex_.Get(), blurTexB_.Get());
        stagingValid_ = true;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const uint64_t mapStartUs = QpcNowUs();
    HRESULT hr = ctx->Map(stagingTex_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return hr;
    const uint64_t cpuStartUs = QpcNowUs();
    GlassStats::Instance().lumaMapWaitUs.fetch_add(cpuStartUs - mapStartUs, std::memory_order_relaxed);
    const auto* pixels = static_cast<const uint8_t*>(mapped.pData);

    const float margin = kBlurMarginCss * dpr;
    uint32_t histogram[256];
    for (const LumaBand& band : bands) {
        // Panel-local physical pixels → blur texture coords (region includes
        // the margin, half resolution)
        const LONG x0 = std::clamp<LONG>(static_cast<LONG>((band.rect.left + margin) / 2), 0, blurW_ - 1);
        const LONG x1 = std::clamp<LONG>(static_cast<LONG>((band.rect.right + margin) / 2), 1, blurW_);
        const LONG y0 = std::clamp<LONG>(static_cast<LONG>((band.rect.top + margin) / 2), 0, blurH_ - 1);
        const LONG y1 = std::clamp<LONG>(static_cast<LONG>((band.rect.bottom + margin) / 2), 1, blurH_);
        if (x1 <= x0 || y1 <= y0) continue;

        memset(histogram, 0, sizeof(histogram));
        double sumB = 0;
        double sumG = 0;
        double sumR = 0;
        // Stride-2 subsampling for large bands: the blurred texture is
        // low-frequency, so statistics match full sampling at 1/4 the CPU cost
        const LONG step = (x1 - x0) * (y1 - y0) > 4096 ? 2 : 1;
        uint32_t count = 0;
        for (LONG y = y0; y < y1; y += step) {
            const uint8_t* row = pixels + y * mapped.RowPitch;
            for (LONG x = x0; x < x1; x += step) {
                const uint8_t* px = row + x * 4;  // BGRA
                sumB += px[0];
                sumG += px[1];
                sumR += px[2];
                const int luma = static_cast<int>((px[2] * 54 + px[1] * 183 + px[0] * 19) >> 8);
                histogram[luma > 255 ? 255 : luma]++;
                count++;
            }
        }
        if (count == 0) continue;
        // Luma p15 / p85 from the histogram
        const auto percentile = [&](float q) -> float {
            const uint32_t target = static_cast<uint32_t>(count * q);
            uint32_t acc = 0;
            for (int v = 0; v < 256; v++) {
                acc += histogram[v];
                if (acc >= target) return static_cast<float>(v);
            }
            return 255.0f;
        };
        LumaBandStats stats;
        stats.id = band.id;
        stats.r = static_cast<float>(sumR / count);
        stats.g = static_cast<float>(sumG / count);
        stats.b = static_cast<float>(sumB / count);
        stats.darkTail = percentile(0.15f);
        stats.lightTail = percentile(0.85f);
        out->push_back(stats);
    }
    ctx->Unmap(stagingTex_.Get(), 0);
    GlassStats::Instance().lumaSamples.fetch_add(1, std::memory_order_relaxed);
    GlassStats::Instance().lumaCpuUs.fetch_add(QpcNowUs() - cpuStartUs, std::memory_order_relaxed);
    return S_OK;
}

void GlassRenderer::RunPasses(ID3D11DeviceContext* ctx, GlassPanel& panel, float dpr) {
    const GlassParams& gp = panel.params();
    const RECT& pb = panel.bounds();
    const float panelW = static_cast<float>(pb.right - pb.left);
    const float panelH = static_cast<float>(pb.bottom - pb.top);
    const float margin = kBlurMarginCss * dpr;

    const float halfMin = std::min(panelW, panelH) / 2.0f;
    const float bezel = std::max(4.0f * dpr, std::min(44.0f * dpr, halfMin * 2.0f / 3.5f));
    const float maxBend = halfMin < 16.0f * dpr ? 0.0f : 1.1f * bezel;

    ShaderParams params{};
    params.glassSize[0] = panelW;
    params.glassSize[1] = panelH;
    params.radius = gp.cornerRadius;
    params.bezel = bezel;
    params.maxBend = maxBend;
    params.dispFactor = gp.displacementScale / 70.0f;
    params.aberration = gp.aberrationIntensity;
    params.saturation = gp.saturation;
    params.margin = margin;
    params.alpha = panel.opacity() * gp.opacity;
    params.hdrSource = regionFormat_ == DXGI_FORMAT_R16G16B16A16_FLOAT ? 1.0f : 0.0f;
    params.whiteScale = gp.sdrWhiteScale;
    const ULONGLONG nowWeatherTick = GetTickCount64();
    if (weatherLastTick_ == 0) weatherLastTick_ = nowWeatherTick;
    const double dtSec = std::min(0.1, static_cast<double>(nowWeatherTick - weatherLastTick_) * 0.001);
    weatherLastTick_ = nowWeatherTick;
    const float speedMul = static_cast<float>(g_weatherFxSpeedPct.load(std::memory_order_relaxed)) * 0.01f;
    const int targetWeatherMode = ResolveActiveWeatherFxMode();
    constexpr float kFadeRate = 3.0f; // ~0.33s smooth cross-fade
    const float fadeStep = static_cast<float>(dtSec) * kFadeRate;
    if (targetWeatherMode == weatherCurMode_) {
        if (targetWeatherMode > 0) {
            weatherFade_ = std::min(1.0f, weatherFade_ + fadeStep);
        } else {
            weatherFade_ = std::max(0.0f, weatherFade_ - fadeStep);
        }
    } else if (weatherCurMode_ == 0) {
        weatherCurMode_ = targetWeatherMode;
        weatherFade_ = std::min(1.0f, fadeStep);
    } else {
        weatherFade_ = std::max(0.0f, weatherFade_ - fadeStep * 1.4f);
        if (weatherFade_ <= 0.001f) {
            weatherCurMode_ = targetWeatherMode;
            weatherFade_ = 0.0f;
        }
    }
    weatherTransitioning_ = (weatherCurMode_ != targetWeatherMode)
        || (targetWeatherMode > 0 && weatherFade_ < 0.999f)
        || (targetWeatherMode == 0 && weatherFade_ > 0.001f);

    const int renderedMode = (weatherFade_ > 0.001f) ? weatherCurMode_ : 0;
    if (renderedMode > 0) {
        weatherAnimTime_ += dtSec * static_cast<double>(speedMul);
        if (weatherAnimTime_ > 36000.0) weatherAnimTime_ -= 36000.0;
    }
    // Smoothstep easing curve for silky iOS-like fade-in / fade-out
    const float easedFade = weatherFade_ * weatherFade_ * (3.0f - 2.0f * weatherFade_);
    params.weatherMode = static_cast<float>(renderedMode);
    params.weatherTime = static_cast<float>(weatherAnimTime_);
    params.weatherIntensity = renderedMode > 0
        ? static_cast<float>(g_weatherFxIntensityPct.load(std::memory_order_relaxed)) * 0.01f * easedFade
        : 0.0f;
    params.weatherDpr = dpr > 0.1f ? dpr : 1.0f;
    params.weatherScale = static_cast<float>(g_weatherFxScalePct.load(std::memory_order_relaxed)) * 0.01f;
    params.weatherFog = static_cast<float>(g_weatherFxFogPct.load(std::memory_order_relaxed)) * 0.01f;
    params.weatherRefract = static_cast<float>(g_weatherFxRefractPct.load(std::memory_order_relaxed)) * 0.01f;
    params.weatherPad = 0.0f;

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, cb_.GetAddressOf());

    const auto setViewport = [&](float w, float h) {
        D3D11_VIEWPORT vp{ 0, 0, w, h, 0, 1 };
        ctx->RSSetViewports(1, &vp);
    };
    ID3D11ShaderResourceView* nullSrv = nullptr;

    // Pass 1: full-res region → half res + horizontal Gaussian
    params.srcOrigin[0] = 0;
    params.srcOrigin[1] = 0;
    params.srcSpan[0] = 1;
    params.srcSpan[1] = 1;
    params.outSize[0] = static_cast<float>(blurW_);
    params.outSize[1] = static_cast<float>(blurH_);
    params.dir[0] = gp.blurSigma / static_cast<float>(regionW_);
    params.dir[1] = 0;
    ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &params, 0, 0);
    ctx->OMSetRenderTargets(1, blurRtvA_.GetAddressOf(), nullptr);
    setViewport(static_cast<float>(blurW_), static_cast<float>(blurH_));
    ctx->PSSetShader(blurPs_.Get(), nullptr, 0);
    ctx->PSSetShaderResources(0, 1, regionSrv_.GetAddressOf());
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 1, &nullSrv);

    // Pass 2: vertical Gaussian
    params.hdrSource = 0;
    params.dir[0] = 0;
    params.dir[1] = gp.blurSigma / static_cast<float>(regionH_);
    ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &params, 0, 0);
    ctx->OMSetRenderTargets(1, blurRtvB_.GetAddressOf(), nullptr);
    ctx->PSSetShaderResources(0, 1, blurSrvA_.GetAddressOf());
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 1, &nullSrv);

    // Extra progressive wide-kernel blur passes only for Fog/Cloudy mode (Rain uses pure original Rain.cs sampling)
    const bool heavyCondensation = (renderedMode == 4)
        && (params.weatherIntensity * params.weatherFog > 0.04f);
    if (heavyCondensation && blurW_ > 0 && blurH_ > 0) {
        const float mistFactor = std::min(1.0f, params.weatherIntensity * params.weatherFog);
        const float extraRadii[3] = { 1.8f, 3.4f, 5.4f };
        for (int pass = 0; pass < 3; ++pass) {
            const float stepPx = extraRadii[pass] * dpr * mistFactor;
            // Horizontal wide step: B -> A
            params.dir[0] = stepPx / static_cast<float>(blurW_);
            params.dir[1] = 0.0f;
            ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &params, 0, 0);
            ctx->OMSetRenderTargets(1, blurRtvA_.GetAddressOf(), nullptr);
            ctx->PSSetShaderResources(0, 1, blurSrvB_.GetAddressOf());
            ctx->Draw(3, 0);
            ctx->PSSetShaderResources(0, 1, &nullSrv);

            // Vertical wide step: A -> B
            params.dir[0] = 0.0f;
            params.dir[1] = stepPx / static_cast<float>(blurH_);
            ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &params, 0, 0);
            ctx->OMSetRenderTargets(1, blurRtvB_.GetAddressOf(), nullptr);
            ctx->PSSetShaderResources(0, 1, blurSrvA_.GetAddressOf());
            ctx->Draw(3, 0);
            ctx->PSSetShaderResources(0, 1, &nullSrv);
        }
    }

    // Queue the luma staging copy as soon as the blur result is ready: the
    // submission and execution of the lens pass / Present hide the copy
    // latency, so the later Map barely waits on the GPU
    if (lumaWanted_ && SUCCEEDED(EnsureStaging(ctx))) {
        ctx->CopyResource(stagingTex_.Get(), blurTexB_.Get());
        stagingValid_ = true;
    }

    // Pass 3: lens shading → panel back buffer
    ID3D11RenderTargetView* backBuffer = panel.AcquireBackBuffer();
    if (!backBuffer) return;
    params.outSize[0] = panelW;
    params.outSize[1] = panelH;
    params.hdrSource = regionFormat_ == DXGI_FORMAT_R16G16B16A16_FLOAT ? 1.0f : 0.0f;
    ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &params, 0, 0);
    ctx->OMSetRenderTargets(1, &backBuffer, nullptr);
    setViewport(panelW, panelH);
    ctx->PSSetShader(lensPs_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* lensSources[] = { blurSrvB_.Get(), regionSrv_.Get() };
    ctx->PSSetShaderResources(0, 2, lensSources);
    ctx->Draw(3, 0);
    lensSources[0] = lensSources[1] = nullptr;
    ctx->PSSetShaderResources(0, 2, lensSources);
    ID3D11RenderTargetView* nullRtv = nullptr;
    ctx->OMSetRenderTargets(1, &nullRtv, nullptr);

    panel.Present();
}
