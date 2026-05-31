# Terrain Layer Mask Technical Document

本文档总结当前 procedural terrain 的所有材质 layer mask 计算方法。当前系统在 compute shader 中生成每个 clipmap tile 的高度、法线和材质权重图，然后在 terrain pixel shader 中消费这些权重完成 PBR 材质混合。

## 1. 数据流概览

### 1.1 生成阶段

入口文件：

- `GlareEngine/Shaders/Terrain/TerrainNoiseCS.hlsl`
- `GlareEngine/Shaders/Terrain/TerrainNoise.hlsli`
- `GlareEngine/Shaders/Terrain/TerrainCommon.hlsli`

每个 tile 运行 `TerrainNoiseCS.hlsl`，输出三张 UAV：

| 输出资源 | 格式语义 | 内容 |
| --- | --- | --- |
| `gOutputHeightMap` | `float` | 归一化高度，写入公式为 `height / (2 * gNoiseHeightScale) + 0.5` |
| `gOutputNormalMap` | `float4` | 世界空间法线 `float4(normal, 1)` |
| `gOutputMaterialWeights` | `float4` | 材质 mask，RGBA 分别存储 grass、lightdirt、darkdirt、stone |

材质层顺序固定为：

| Layer Index | Layer Name | Weight Source |
| --- | --- | --- |
| 0 | `grass` | `weights.r` |
| 1 | `lightdirt` | `weights.g` |
| 2 | `darkdirt` | `weights.b` |
| 3 | `stone` | `weights.a` |
| 4 | `snow` | shader 中由 `1 - r - g - b - a` 推导 |

### 1.2 消费阶段

入口文件：

- `GlareEngine/Shaders/Terrain/TerrainClipmapDS.hlsl`
- `GlareEngine/Shaders/Terrain/TerrainClipmapDeferredPS.hlsl`
- `GlareEngine/Shaders/Terrain/TerrainCommon.hlsli`

Domain shader 通过 `SampleMaterialWeights(hmUV)` 读取 material weight map，并将 `MatWeights` 传给 pixel shader。Deferred pixel shader 中：

```hlsl
w[0] = weights.r;
w[1] = weights.g;
w[2] = weights.b;
w[3] = weights.a;
w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]);
```

因此 snow 没有单独通道，占用 RGBA 剩余权重。

## 2. 公共输入参数

### 2.1 Tile 坐标和高度

每个 compute thread 对应 material/height map 上一个 texel：

```hlsl
float2 tileUV = (float2)DTid.xy / max((float)(gNoiseHeightmapSize - 1), 1.0);
float2 worldXZ = ((float2)gNoiseTileOffset + tileUV * (float)gNoiseTileSize) * gNoiseCellSize;
```

高度由 procedural noise stack 计算：

```hlsl
float3 result = ComputeTerrainHeightWithDerivatives(worldXZ, gNoiseHeightScale);
float height = result.x;
float2 gradient = result.yz;
```

`ComputeTerrainHeightWithDerivatives` 内部使用 base layers 和 detail layers 两组 noise stack：

- Base layers: `gNoiseLayerControls[0..3]`
- Detail layers: `gNoiseLayerControls[4..7]`

两者相加后乘以 `gNoiseHeightScale`，并 clamp 到 `[-gNoiseHeightScale, gNoiseHeightScale]`。

### 2.2 坡度

法线从高度梯度得到：

```hlsl
float3 normal = normalize(float3(-gradient.x, 1.0, -gradient.y));
float slope = 1.0 - normal.y;
```

语义：

- `slope = 0`：完全水平
- `slope = 1`：接近垂直

后续 stone、grass 等 layer 都基于这个 slope 做地形约束。

### 2.3 Mask 控制参数

当前 C++ constant buffer 和 HLSL cbuffer 中的相关参数为：

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `SnowHeight` / `gNoiseSnowHeight` | `150.0` | 雪线中心高度 |
| `SnowTransition` / `gNoiseSnowTransition` | `20.0` | 雪线过渡宽度 |
| `StoneSlope` / `gNoiseStoneSlope` | `0.6` | stone 坡度阈值中心 |
| `StoneTransition` / `gNoiseStoneTransition` | `0.15` | stone 坡度过渡宽度 |
| `GrassCoverage` / `gGrassCoverage` | `1.0` | grass 总覆盖强度 |
| `GrassPatchiness` / `gGrassPatchiness` | `0.55` | grass 斑块化程度 |
| `GrassMaxSlope` / `gGrassMaxSlope` | `0.35` | grass 最大适宜坡度 |
| `GrassMoistureBias` / `gGrassMoistureBias` | `0.45` | 凹地/湿润地形对 grass 的增强强度 |

UI 中可调参数位于 Terrain 面板的 `Materials` tree node。

## 3. 各 Layer Raw Weight 计算

`ComputeMaterialWeights(height, slope, worldXZ)` 会先计算每个 layer 的 raw weight，然后统一归一化。核心辅助噪声：

```hlsl
float macroNoise = TerrainGradientNoise(worldXZ * 0.008);
```

`macroNoise` 用于打破纯高度带造成的水平条带，主要影响 stone 高度线和 dark dirt valley 线。

## 4. Snow Mask

Snow 不是单纯的高度全覆盖。当前实现先用高度得到基础雪线，再叠加表面坡度积雪能力，以及只作用在雪线边缘的噪声破碎。

```hlsl
float baseSnow = smoothstep(
    snowH - gNoiseSnowTransition,
    snowH + gNoiseSnowTransition,
    height);

float snowEdgeMask = baseSnow * (1.0 - baseSnow) * 4.0;
float snowEdgeNoise = TerrainGradientNoise(worldXZ * 0.014) * gNoiseSnowTransition * 0.8;
float snowBreakupNoise = TerrainGradientNoise(worldXZ * 0.075) * 0.5 + 0.5;
float snowAccumulation = 1.0 - smoothstep(0.18, 0.75, slope);

float snow = smoothstep(
    snowH - gNoiseSnowTransition,
    snowH + gNoiseSnowTransition,
    height + snowEdgeNoise);

snow *= snowAccumulation;
snow = saturate(snow + (snowBreakupNoise - 0.5) * snowEdgeMask * 0.35 * snowAccumulation);
```

语义：

- 当 `height <= snowH - transition` 时，snow 接近 `0`。
- 当 `height >= snowH + transition` 时，snow 接近 `1`。
- 中间使用 `smoothstep` 平滑过渡。
- `snowEdgeMask` 只在雪线过渡区接近 1，在无雪区和雪地核心区接近 0。
- `snowEdgeNoise` 用低频 world-space 噪声扰动雪线高度，让雪地边缘不再沿等高线规则展开。
- `snowAccumulation` 根据坡度控制积雪能力：缓坡和平台容易积雪，陡坡会露出 stone/lightdirt。
- `snowBreakupNoise` 只通过 `snowEdgeMask` 轻微破碎边缘，不影响雪地核心。

设计目的：

- 高海拔区域逐渐进入 snow。
- 过渡带宽度由 `SnowTransition` 控制。
- 高海拔陡峭岩壁不会被雪完全覆盖。
- 雪地边缘具有随机、自然的破碎形状。
- snow 最终不写入 RGBA，而是由 deferred shader 中的剩余权重推导。

注意：

- Compute 阶段参与归一化时，snow 是第五个权重 `w[4]`。
- 写入 texture 时只写 `w[0..3]`，snow 在 pixel shader 中恢复为 `1 - sum(rgba)`。

## 5. Stone Mask

Stone 同时由坡度和高海拔控制。

### 5.1 坡度 stone

```hlsl
float slopeStone = smoothstep(
    gNoiseStoneSlope - gNoiseStoneTransition,
    gNoiseStoneSlope + gNoiseStoneTransition,
    slope);
```

语义：

- 坡度低于阈值下沿时 stone 较少。
- 坡度高于阈值上沿时 stone 接近满值。
- `StoneSlope` 越低，岩石越容易出现在较缓坡面。

### 5.2 高度 stone

```hlsl
float heightStone = smoothstep(
    snowH * 0.7,
    snowH * 0.95,
    height + macroNoise * snowH * 0.2);
```

语义：

- 在雪线以下的较高海拔区域逐渐增加 stone。
- `macroNoise` 对高度输入做偏移，避免 stone/snow/grass 过渡呈现规则等高线。

### 5.3 Stone 合成

```hlsl
float stoneBase = max(slopeStone, heightStone) * (1.0 - snow) * 1.35;

float rockCreviceNoise = TerrainGradientNoise(worldXZ * 0.055);
float rockCreviceRidges = 1.0 - abs(rockCreviceNoise);
float screeNoise = TerrainGradientNoise(worldXZ * 0.16) * 0.5 + 0.5;
float rockCreviceMask = smoothstep(0.62, 0.92, rockCreviceRidges)
    * (0.55 + 0.45 * screeNoise)
    * saturate(stoneBase)
    * (0.35 + 0.65 * saturate(slopeStone))
    * (1.0 - snow);
float scree = rockCreviceMask * 0.55;
float stone = stoneBase * (1.0 - scree * 0.65);
```

语义：

- 坡度和高度任一条件强，就提高 stone。
- snow 区域压制 stone，避免 snow 与 stone 同时占满。
- `1.35` 是强度放大，让陡坡和高海拔裸岩在归一化竞争中更有优势。
- `rockCreviceMask` 只在 stone 已经较强的区域内出现，用 ridged noise 模拟岩石裂隙。
- `scree` 从 stone 中让出一部分权重，后续转给 lightdirt，用当前材质资源模拟碎石/岩屑填缝。
- 这里不使用 darkdirt 表示岩缝，因为 darkdirt 更适合低洼湿润泥土；岩石裂隙中的碎石应更接近 lightdirt/scree 的视觉。

## 6. Grass Mask

Grass 使用独立 helper：

```hlsl
float grass = ComputeGrassSuitability(height, slope, worldXZ)
    * (1.0 - snow)
    * (1.0 - saturate(slopeStone) * 0.85);
```

总体目标是模拟高山草甸分布：草集中在雪线以下、中高海拔缓坡、凹地、坡脚和连续台地；陡坡、凸脊、裸岩、雪线以上区域减少草。

### 6.1 Alpine height band

```hlsl
float lowerBand = smoothstep(-snowH * 0.35, snowH * 0.05, height);
float upperBand = 1.0 - smoothstep(snowH * 0.52, snowH * 0.82, height);
float alpineBand = lowerBand * upperBand;
```

语义：

- `lowerBand`：排除过低区域，让草从低地/谷底逐渐出现。
- `upperBand`：接近雪线高处逐渐减少草。
- 两者相乘形成中低到中高海拔之间的草甸适宜高度带。

### 6.2 Slope habitat

```hlsl
float slopeLimit = max(gGrassMaxSlope, 0.05);
float slopeHabitat = 1.0 - smoothstep(slopeLimit * 0.55, slopeLimit, slope);
```

语义：

- 缓坡适宜 grass。
- 接近 `GrassMaxSlope` 时 grass 快速衰减。
- `GrassMaxSlope` 越大，草可覆盖更陡区域。

### 6.3 Meadow patch

```hlsl
float patchiness = saturate(gGrassPatchiness);
float macroPatchNoise = TerrainGradientNoise(worldXZ * 0.004) * 0.5 + 0.5;
float patchThreshold = lerp(0.20, 0.62, patchiness);
float patchSoftness = lerp(0.35, 0.12, patchiness);
float meadowPatch = smoothstep(
    patchThreshold - patchSoftness,
    patchThreshold + patchSoftness,
    macroPatchNoise);
```

语义：

- 使用低频 noise 生成大尺度连续草甸斑块。
- `GrassPatchiness` 越高：
  - threshold 越高，草甸出现条件更严格。
  - softness 越低，边界更清晰。
  - 结果更破碎、更斑块化。
- `GrassPatchiness` 越低：
  - 草更连续。
  - 边界更柔和。

### 6.4 Curvature / moisture

曲率估算：

```hlsl
float sampleStep = max(gNoiseCellSize * 8.0, 1.0);
float hL = ComputeTerrainHeight(worldXZ + float2(-sampleStep, 0.0), gNoiseHeightScale);
float hR = ComputeTerrainHeight(worldXZ + float2(sampleStep, 0.0), gNoiseHeightScale);
float hD = ComputeTerrainHeight(worldXZ + float2(0.0, -sampleStep), gNoiseHeightScale);
float hU = ComputeTerrainHeight(worldXZ + float2(0.0, sampleStep), gNoiseHeightScale);
float neighborAverage = (hL + hR + hD + hU) * 0.25;
float curvature = clamp(
    (neighborAverage - height) / max(gNoiseHeightScale * 0.08, 1.0),
    -1.0,
    1.0);
```

语义：

- `curvature > 0`：当前位置低于周围平均高度，倾向于凹地、沟谷、坡脚，模拟更湿润区域。
- `curvature < 0`：当前位置高于周围平均高度，倾向于凸脊、山脊，模拟更干、更裸露区域。

湿度影响：

```hlsl
float moisture = lerp(0.65, 1.25, smoothstep(-0.35, 0.55, curvature));
float moistureInfluence = lerp(1.0, moisture, saturate(gGrassMoistureBias));
```

语义：

- 凹地提高 grass。
- 凸脊降低 grass。
- `GrassMoistureBias = 0` 时禁用该地形湿度影响。
- `GrassMoistureBias = 1` 时完整应用湿度 multiplier。

### 6.5 Edge breakup

```hlsl
float fineNoise = TerrainGradientNoise(worldXZ * 0.035) * 0.5 + 0.5;
float edgeBreakup = lerp(
    1.0,
    smoothstep(0.25, 0.75, fineNoise),
    patchiness * 0.35);
```

语义：

- 高频噪声只轻度影响 grass。
- 影响强度最多为 `patchiness * 0.35`，避免把草甸中心打成随机碎点。
- 主要用于草甸边缘的自然破碎。

### 6.6 Grass suitability 输出

```hlsl
return alpineBand
    * slopeHabitat
    * meadowPatch
    * moistureInfluence
    * edgeBreakup
    * max(gGrassCoverage, 0.0);
```

最后在 `ComputeMaterialWeights` 中再乘：

```hlsl
* (1.0 - snow)
* (1.0 - saturate(slopeStone) * 0.85)
```

这两个额外项用于：

- 避免 snow 区域出现 grass。
- 在强陡坡 stone 区域压低 grass，但不是完全硬切，保留过渡。

## 7. Dark Dirt Mask

Dark dirt 表示低洼、谷底、较湿或较暗的泥土区域：

```hlsl
float darkDirtHeight = 1.0 - smoothstep(
    -snowH * 0.5,
    -snowH * 0.1,
    height + macroNoise * snowH * 0.1);

float darkDirt = darkDirtHeight
    * (1.0 - snow)
    * (1.0 - saturate(stone))
    * 0.9;
```

语义：

- 低海拔更容易出现 dark dirt。
- `macroNoise` 轻微扰动高度边界，避免规则水平带。
- snow 和 stone 区域压制 dark dirt。
- `0.9` 略微降低 dark dirt 的竞争强度，给 grass/lightdirt 留出空间。

## 8. Light Dirt Mask

Light dirt 是默认补位 layer，用于非 snow、非 stone、非 grass 的普通裸土或稀疏地表：

```hlsl
float lightDirt = max(
    0.05,
    (1.0 - snow)
    * (1.0 - saturate(stone))
    * (1.0 - saturate(grass))
    * 0.65);

lightDirt += scree * stoneBase;
```

语义：

- 在 snow、stone、grass 之外补充基础地表。
- 至少保留 `0.05` raw weight，避免所有非 snow layer 都接近 0 时归一化不稳定。
- grass 越强，light dirt 越弱。
- stone/snow 越强，light dirt 越弱。
- stone 内部的 `scree` 会额外增加 light dirt，用 lightdirt 材质代理碎石/岩屑，而不是把岩缝表现成湿泥。

## 9. 归一化和 RGBA 打包

所有 raw weight 计算完成后：

```hlsl
float w[5];
w[0] = grass;
w[1] = lightDirt;
w[2] = darkDirt;
w[3] = stone;
w[4] = snow;

float sum = w[0] + w[1] + w[2] + w[3] + w[4] + 0.0001;
return float4(w[0] / sum, w[1] / sum, w[2] / sum, w[3] / sum);
```

特点：

- grass/lightdirt/darkdirt/stone 写入 RGBA。
- snow 不写入 texture，通过剩余权重恢复。
- `0.0001` 防止除 0。
- 因为 RGBA 写入已经除以包含 snow 在内的总和，所以 `r + g + b + a <= 1`，剩余量为 snow。

## 10. Pixel Shader 中的二次权重调整

`TerrainClipmapDeferredPS.hlsl` 并不直接用 material weight 做最终混合，而是先采样每个 layer 的 PBR 材质，再根据材质 height/luminance 进行 height-based adjustment。

### 10.1 初始权重恢复

```hlsl
w[0] = weights.r; // grass
w[1] = weights.g; // lightdirt
w[2] = weights.b; // darkdirt
w[3] = weights.a; // stone
w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow
```

### 10.2 Layer 采样

每个 active layer 采样：

- albedo
- normal
- roughness
- metallic
- AO
- optional height

平面 base layer 采样使用 stochastic：

```hlsl
TerrainSamplePlanarRGB(..., false)
TerrainSamplePlanarScalar(..., false)
```

detail overlay 和 height map 采样使用 aligned/direct sampling：

```hlsl
TerrainSamplePlanarRGB(..., true)
TerrainSamplePlanarScalar(..., true)
```

这样 base layer 使用 stochastic strength 降低 tiling，detail layer 不受 stochastic strength 影响。

### 10.3 Triplanar

如果开启 `gTerrainUseTriplanar`，shader 根据法线 upness 计算 `triplanarFade`：

```hlsl
float triplanarFade = TerrainTriplanarFade(surfaceNormalW);
```

然后在 planar projection 和 triplanar projection 之间 lerp。陡坡更倾向 triplanar，以减少垂直坡面拉伸。

### 10.4 Height-based layer adjustment

如果 `gTerrainUseHeightBlend` 开启且 layer 有 height map：

```hlsl
layerHeight[i] = sampledHeight;
```

否则 fallback 到 albedo luminance：

```hlsl
layerHeight[i] = dot(albedo, float3(0.299, 0.587, 0.114));
```

最终调整：

```hlsl
float blendSharpness = 2.0;
adjustedW[j] = w[j] * (layerHeight[j] * blendSharpness + 0.01);
```

语义：

- 原始 material mask 决定“哪些 layer 应该出现”。
- height/luminance 决定 transition zone 中“哪个材质 texel 更容易穿透”。
- 这样可以让石头、泥土、草地边界更自然，减少单纯线性混合的糊感。

调整后再次归一化：

```hlsl
float fw = adjustedW[k] / totalW;
result.Albedo += layerAlbedo[k] * fw;
...
```

## 11. 参数调试建议

### 11.1 Snow

- `Snow Height` 增大：雪线升高，snow 减少，高海拔 stone/grass/lightdirt 增加。
- `Snow Height` 降低：雪线降低，snow 覆盖更多区域。

### 11.2 Stone

- `Stone Slope` 增大：只有更陡坡才变 stone。
- `Stone Slope` 降低：更多中等坡度区域变 stone，grass 被压缩。

### 11.3 Grass

- `Grass Coverage` 增大：grass 整体更强，R 通道更亮。
- `Grass Coverage` 降低：grass 整体减少，lightdirt/darkdirt/stone 相对增加。
- `Grass Patchiness` 增大：grass 更斑块化，边界更碎，覆盖更不连续。
- `Grass Patchiness` 降低：grass 更连续，草甸更成片。
- `Grass Max Slope` 增大：grass 可以覆盖更陡的坡。
- `Grass Max Slope` 降低：grass 更集中在缓坡和台地。
- `Grass Moisture Bias` 增大：凹地、坡脚、沟谷 grass 增强，山脊 grass 减少。
- `Grass Moisture Bias` 降低：减少曲率/湿度影响，grass 更由高度、坡度和 patch noise 决定。

## 12. Debug 和验收方法

推荐使用 Terrain debug texture：

```cpp
EngineGUI::AddRenderPassVisualizeTexture("Terrain", "MaterialWeights", ...)
```

检查 `MaterialWeights`：

- R 通道：grass
- G 通道：lightdirt
- B 通道：darkdirt
- A 通道：stone
- snow：不是显式通道，需要通过 `1 - R - G - B - A` 推断

视觉验收重点：

- grass 应集中在中高海拔缓坡、台地、坡脚和凹地。
- 陡坡和山脊应更多转向 stone/lightdirt。
- 雪线以上不应有明显 grass。
- grass 斑块应连续，不应呈现均匀随机噪点。
- tile 边界不应出现 mask 断裂，因为所有 mask 使用 world-space 坐标计算。

## 13. 关键约束

- material weight map 只有 RGBA 四个通道，因此 snow 必须继续使用剩余权重编码。
- C++ `ProceduralTerrainNoiseCB` 与 HLSL `TerrainNoiseCB` 字段顺序必须保持一致。
- 修改任何 mask 参数后，需要进入 `ComputeNoiseLayerStateHash()`，否则 UI 调参不会触发 tile regenerate。
- 如果新增 layer 或改变 RGBA 语义，需要同步修改：
  - material texture loading order
  - compute shader packing
  - domain shader sampling
  - deferred pixel shader layer decoding
  - debug texture interpretation
