# Cesium 运行时 Nanite 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 VoxelCore 的运行时 Nanite Builder 最小化移植到 Cesium Runtime，使所有符合条件的 Cesium 三角形图元可选地使用运行时 Nanite，并在任何不支持或失败情况下自动回退现有 Legacy 网格路径。

**Architecture:** 在 Cesium 已完成 glTF Accessor 验证和坐标转换后，先生成一份与渲染后端无关的公共图元网格数据，再由纯资格策略选择 Runtime Nanite 或 Legacy Builder。Nanite Cluster/Page 编码在 Cesium 工作线程完成，临时 `UStaticMesh`、动态材质、碰撞、元数据和资源初始化仍沿用现有游戏线程流程。

**Tech Stack:** Unreal Engine 5.7、C++20、Nanite Runtime Resources、Cesium for Unreal、Unreal Automation Testing、VoxelCore MIT Builder（固定参考提交 `d3910625a3bf295f60b52c84d13e43177991198b`）

---

## 文件结构

新增文件：

- `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteTypes.h`
  - 定义公共图元网格输入、Nanite Builder 输入、资格判断输入/结果及回退原因。
- `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEligibility.h/.cpp`
  - 实现无 UObject、无 RHI 查询副作用的纯资格判断。
- `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEncoding.h/.cpp`
  - 移植 VoxelCore Cluster、Page、量化及编码基础设施。
- `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteBuilder.h/.cpp`
  - 接受公共网格数据并创建含 `Nanite::FResources` 的
    `FStaticMeshRenderData`。
- `Source/CesiumRuntime/Private/RuntimeNanite/LICENSE.VoxelCore.txt`
  - 保存 VoxelCore MIT 许可证文本和固定来源提交。
- `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`
  - 测试配置默认值、资格判断、Builder 产物和 Legacy 回退。

修改文件：

- `Source/CesiumRuntime/CesiumRuntime.Build.cs`
  - 增加 Nanite Builder 所需的最小 Engine/Renderer 私有包含路径。
- `Source/CesiumRuntime/Public/Cesium3DTileset.h`
  - 增加默认关闭的 Runtime Nanite 属性、Getter 和 Setter。
- `Source/CesiumRuntime/Private/Cesium3DTileset.cpp`
  - 实现 Setter、编辑器属性变更重载和配置传递。
- `Source/CesiumRuntime/Private/CreateGltfOptions.h`
  - 将 Actor 配置传递到工作线程图元加载。
- `Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp`
  - 从 `ACesium3DTileset` 填充 Nanite 配置。
- `Source/CesiumRuntime/Private/LoadGltfResult.h`
  - 记录图元最终使用的渲染路径。
- `Source/CesiumRuntime/Private/CesiumGltfComponent.cpp`
  - 提取公共网格数据、分流 Builder、配置临时 `UStaticMesh`。

不要修改：

- `extern/cesium-native`
- Linux 工具链和 vcpkg triplet
- `CesiumRasterOverlays.h`
- 任何 build 目录

---

### Task 0：准备隔离的 UE 5.7 测试工程

**Files:**

- External test project:
  `C:\tmp\cesium-runtime-nanite-host\HostProject.uproject`
- External plugin copy:
  `C:\tmp\cesium-runtime-nanite-host\Plugins\CesiumForUnreal`

- [ ] **Step 1：从已验证的临时 HostProject 建立副本**

以现有的：

```text
C:\tmp\cesium-holo-overlay-host
```

为模板复制到：

```text
C:\tmp\cesium-runtime-nanite-host
```

删除副本中的 `Binaries`、`Intermediate` 和 `Saved`，保留 `.uproject` 和必要配置。

- [ ] **Step 2：同步当前隔离 worktree 插件**

将：

```text
E:\UnrealProjects\cesium-unreal\.worktrees\runtime-nanite
```

同步到：

```text
C:\tmp\cesium-runtime-nanite-host\Plugins\CesiumForUnreal
```

排除：

```text
.git
.worktrees
build*
extern/build*
testdata
```

- [ ] **Step 3：验证干净基线可编译**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  UnrealEditor Win64 Development `
  -Project='C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -WaitMutex -NoHotReloadFromIDE
```

Expected: UHT 和 `UnrealEditor-CesiumRuntime.dll` 编译成功，退出码 0。

- [ ] **Step 4：验证测试发现基线**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -unattended -nop4 -NullRHI `
  -ExecCmds='Automation List;Quit' -log
```

Expected: 能发现现有 `Cesium.Unit` 测试，退出码 0。

---

### Task 1：增加 Actor 配置及参数传递

**Files:**

- Modify: `Source/CesiumRuntime/Public/Cesium3DTileset.h`
- Modify: `Source/CesiumRuntime/Private/Cesium3DTileset.cpp`
- Modify: `Source/CesiumRuntime/Private/CreateGltfOptions.h`
- Modify: `Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp`
- Create: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：编写默认值失败测试**

创建自动化测试：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumRuntimeNaniteDefaults,
    "Cesium.Unit.RuntimeNanite.Defaults",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ProductFilter);

bool FCesiumRuntimeNaniteDefaults::RunTest(const FString&) {
  const ACesium3DTileset* pTileset = GetDefault<ACesium3DTileset>();
  TestFalse(
      TEXT("Runtime Nanite is disabled by default"),
      pTileset->GetEnableRuntimeNanite());
  TestEqual(
      TEXT("Runtime Nanite defaults to 2000 triangles"),
      pTileset->GetRuntimeNaniteMinimumTriangleCount(),
      2000);
  return true;
}
```

- [ ] **Step 2：运行编译验证 RED**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  UnrealEditor Win64 Development `
  -Project='C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -WaitMutex -NoHotReloadFromIDE
```

Expected: FAIL，提示 `GetEnableRuntimeNanite` 和
`GetRuntimeNaniteMinimumTriangleCount` 不存在。

- [ ] **Step 3：增加反射属性和 Getter/Setter**

在 `ACesium3DTileset` 的 Rendering 配置附近增加：

```cpp
UPROPERTY(
    EditAnywhere,
    BlueprintGetter = GetEnableRuntimeNanite,
    BlueprintSetter = SetEnableRuntimeNanite,
    Category = "Cesium|Rendering")
bool EnableRuntimeNanite = false;

UPROPERTY(
    EditAnywhere,
    BlueprintGetter = GetRuntimeNaniteMinimumTriangleCount,
    BlueprintSetter = SetRuntimeNaniteMinimumTriangleCount,
    Category = "Cesium|Rendering",
    meta = (ClampMin = 0))
int32 RuntimeNaniteMinimumTriangleCount = 2000;
```

Setter 仅在值变化时调用 `DestroyTileset()`；三角形阈值使用
`FMath::Max(0, Value)`。

- [ ] **Step 4：把配置传到 CreateModelOptions**

在 `CreateGltfOptions::CreateModelOptions` 增加：

```cpp
bool enableRuntimeNanite = false;
int32 runtimeNaniteMinimumTriangleCount = 2000;
```

更新移动构造函数，确保字段不会在异步链中丢失。

在 `UnrealPrepareRendererResources::prepareInLoadThread` 中设置：

```cpp
options.enableRuntimeNanite =
    this->_pActor->GetEnableRuntimeNanite();
options.runtimeNaniteMinimumTriangleCount =
    this->_pActor->GetRuntimeNaniteMinimumTriangleCount();
```

- [ ] **Step 5：将属性加入编辑器重建条件**

在 `PostEditChangeProperty` 的重载属性集合中加入两个新属性。

- [ ] **Step 6：运行 Defaults 测试验证 GREEN**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -unattended -nop4 -NullRHI `
  -ExecCmds='Automation RunTests Cesium.Unit.RuntimeNanite.Defaults;Quit' `
  -TestExit='Automation Test Queue Empty' -log
```

Expected: 1 test found，测试成功，退出码 0。

- [ ] **Step 7：提交配置改动**

```powershell
git add -- `
  Source/CesiumRuntime/Public/Cesium3DTileset.h `
  Source/CesiumRuntime/Private/Cesium3DTileset.cpp `
  Source/CesiumRuntime/Private/CreateGltfOptions.h `
  Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "feat: add runtime Nanite tileset settings"
```

---

### Task 2：实现可独立测试的资格判断策略

**Files:**

- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteTypes.h`
- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEligibility.h`
- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEligibility.cpp`
- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：编写资格判断失败测试**

用纯输入结构覆盖：

```cpp
struct FCesiumRuntimeNaniteEligibilityInput {
  bool bActorEnabled = false;
  bool bGlobalEnabled = true;
  bool bPlatformSupported = false;
  bool bIsTriangleList = false;
  bool bIsTranslucent = false;
  bool bHasValidPositions = false;
  bool bHasValidIndices = false;
  bool bHasValidNormals = false;
  int32 TriangleCount = 0;
  int32 MinimumTriangleCount = 2000;
  int32 NumTextureCoordinates = 0;
  int32 MaximumTextureCoordinates = NANITE_MAX_UVS;
};
```

逐项验证以下结果：

- Actor 关闭 → `DisabledByActor`
- 全局关闭 → `DisabledGlobally`
- 平台不支持 → `UnsupportedPlatform`
- 非三角形列表 → `UnsupportedPrimitiveMode`
- Translucent → `TranslucentMaterial`
- 数据无效 → 对应无效原因
- 小于阈值 → `BelowTriangleThreshold`
- UV 过多 → `TooManyTextureCoordinates`
- 全部满足 → Eligible

- [ ] **Step 2：运行测试验证 RED**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -unattended -nop4 -NullRHI `
  -ExecCmds='Automation RunTests Cesium.Unit.RuntimeNanite.Eligibility;Quit' `
  -TestExit='Automation Test Queue Empty' -log
```

Expected: FAIL，因为资格类型和函数尚不存在。

- [ ] **Step 3：实现纯资格函数**

公开一个无副作用函数：

```cpp
FCesiumRuntimeNaniteEligibilityResult
EvaluateCesiumRuntimeNaniteEligibility(
    const FCesiumRuntimeNaniteEligibilityInput& Input);
```

按固定顺序返回第一个回退原因，便于日志和测试稳定。

- [ ] **Step 4：增加 CVar 读取边界**

在 `.cpp` 中定义：

```text
cesium.RuntimeNanite.Enable
cesium.RuntimeNanite.MinTriangles
cesium.RuntimeNanite.LogFallbacks
```

另外提供只负责收集运行时状态的函数：

```cpp
FCesiumRuntimeNaniteRuntimeSettings
GetCesiumRuntimeNaniteRuntimeSettings(
    bool bActorEnabled,
    int32 ActorMinimumTriangles);
```

平台支持使用：

```cpp
UseNanite(GMaxRHIShaderPlatform)
```

纯资格测试不直接依赖全局 RHI。

- [ ] **Step 5：运行资格测试验证 GREEN**

Expected: 全部资格分支通过。

- [ ] **Step 6：提交策略层**

```powershell
git add -- `
  Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteTypes.h `
  Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEligibility.h `
  Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEligibility.cpp `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "feat: add runtime Nanite eligibility policy"
```

---

### Task 3：移植 VoxelCore Nanite 编码基础设施

**Files:**

- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEncoding.h`
- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteEncoding.cpp`
- Create: `Source/CesiumRuntime/Private/RuntimeNanite/LICENSE.VoxelCore.txt`
- Modify: `Source/CesiumRuntime/CesiumRuntime.Build.cs`
- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：添加编码层编译测试**

在测试文件中包含 `CesiumRuntimeNaniteEncoding.h`，并验证基础法线八面体编码：

```cpp
const FCesiumRuntimeNaniteOctahedron Encoded =
    FCesiumRuntimeNaniteOctahedron::Encode(FVector3f::UpVector);
TestTrue(TEXT("Encoded normal is finite"), Encoded.IsValid());
```

- [ ] **Step 2：运行编译验证 RED**

Expected: FAIL，因为编码头不存在。

- [ ] **Step 3：复制许可证并记录来源**

`LICENSE.VoxelCore.txt` 包含完整 MIT 文本，并注明：

```text
Source: https://github.com/VoxelPlugin/VoxelCore
Reference commit: d3910625a3bf295f60b52c84d13e43177991198b
Files adapted: VoxelNanite.h, VoxelNanite.cpp,
VoxelNaniteBuilder.h, VoxelNaniteBuilder.cpp
```

- [ ] **Step 4：移植编码结构**

从 VoxelCore 固定提交移植以下概念：

- Octahedron Normal 编码
- Position/UV 量化
- Cluster 数据
- Page Section 尺寸和偏移
- Packed Cluster
- Page Data 编码
- Fixup Chunk 数据

使用 Unreal 原生 `TArray`、`TArrayView`、`TStaticArray` 和基础类型替换所有
`TVoxel*` 容器，不引入 `VoxelMinimal.h` 或 VoxelCore 模块依赖。

所有移植文件头部保留：

```cpp
// Portions derived from VoxelCore.
// Copyright Voxel Plugin SAS.
// Licensed under the MIT License; see LICENSE.VoxelCore.txt.
```

- [ ] **Step 5：更新 Build.cs**

已有 Renderer Private/Internal 路径保持不变，补充 Builder 实际需要的 Engine
私有路径：

```csharp
Path.Combine(GetModuleDirectory("Engine"), "Private"),
Path.Combine(GetModuleDirectory("Engine"), "Internal")
```

不增加完整 VoxelCore 依赖。只有编译器明确要求时才增加新的 Unreal 模块。

- [ ] **Step 6：编译并运行编码测试**

Expected: UE 5.7 编译通过，基础编码测试通过。

- [ ] **Step 7：提交编码基础设施**

```powershell
git add -- `
  Source/CesiumRuntime/CesiumRuntime.Build.cs `
  Source/CesiumRuntime/Private/RuntimeNanite `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "feat: port runtime Nanite encoding"
```

---

### Task 4：实现独立 Runtime Nanite Builder

**Files:**

- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteBuilder.h`
- Create: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteBuilder.cpp`
- Modify: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteTypes.h`
- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：编写最小三角形 Builder 失败测试**

创建一个三角形输入：

```cpp
FCesiumRuntimeNaniteMeshData Mesh;
Mesh.Positions = {
    FVector3f(0, 0, 0),
    FVector3f(100, 0, 0),
    FVector3f(0, 100, 0)};
Mesh.Normals.Init(FVector3f::UpVector, 3);
Mesh.Indices = {0, 1, 2};

TUniquePtr<FStaticMeshRenderData> RenderData =
    BuildCesiumRuntimeNaniteRenderData(Mesh);

TestNotNull(TEXT("Nanite render data is created"), RenderData.Get());
TestTrue(
    TEXT("Nanite resources are valid"),
    RenderData && RenderData->NaniteResourcesPtr.IsValid());
```

- [ ] **Step 2：运行测试验证 RED**

Expected: FAIL，因为 Builder 尚不存在。

- [ ] **Step 3：实现 Builder**

移植并适配 VoxelCore `FVoxelNaniteBuilder`：

- 输入只接受 `TRIANGLES`。
- 验证位置、法线、颜色和每组 UV 的顶点数一致。
- 按 Nanite 常量划分 Cluster。
- 创建 Root Page、Hierarchy Node、Fixup 和
  `Nanite::FPageStreamingState`。
- 设置：

```cpp
RenderData->Bounds = Mesh.Bounds;
RenderData->NumInlinedLODs = 1;
RenderData->NaniteResourcesPtr =
    MakePimpl<Nanite::FResources>(MoveTemp(Resources));
```

- 创建 VoxelCore 同类的最小 Dummy LOD，保证
  `UStaticMesh::HasValidRenderData()` 和 Scene Proxy 不崩溃。
- Builder 只返回数据，不创建 UObject，不调用 `InitResources()`。

- [ ] **Step 4：增加无效输入测试**

验证以下输入返回 `nullptr` 而不是断言崩溃：

- 空位置；
- 法线数量不匹配；
- 索引不是 3 的倍数；
- 索引越界；
- UV 顶点数量不匹配；
- 超出 `NANITE_MAX_UVS`。

- [ ] **Step 5：运行 Builder 测试验证 GREEN**

Expected: 最小三角形产生有效 `NaniteResourcesPtr`，无效输入安全失败。

- [ ] **Step 6：提交 Builder**

```powershell
git add -- `
  Source/CesiumRuntime/Private/RuntimeNanite `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "feat: add runtime Nanite builder"
```

---

### Task 5：从现有加载代码提取公共图元网格数据

**Files:**

- Modify: `Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteTypes.h`
- Modify: `Source/CesiumRuntime/Private/CesiumGltfComponent.cpp`
- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：增加 Legacy 等价性测试**

使用 `CesiumGltfSpecUtility` 构建含位置、法线、颜色、两组 UV 和索引的 glTF
图元。测试公共网格提取结果：

- 坐标执行与现有代码一致的 Y 轴翻转和缩放；
- 索引数量和绕序一致；
- 法线转换一致；
- 两组 UV 完整保留；
- Bounds 与旧路径一致。

- [ ] **Step 2：运行测试验证 RED**

Expected: FAIL，因为公共网格提取函数尚不存在。

- [ ] **Step 3：定义公共网格数据**

```cpp
struct FCesiumPrimitiveMeshData {
  TArray<FVector3f> Positions;
  TArray<FVector3f> Normals;
  TArray<FVector3f> TangentsX;
  TArray<float> TangentSigns;
  TArray<FColor> Colors;
  TArray<TArray<FVector2f>> TextureCoordinates;
  TArray<uint32> Indices;
  FBoxSphereBounds Bounds;
  bool bHasColors = false;
};
```

Builder 视图应直接引用该结构，避免再次复制。

- [ ] **Step 4：拆分现有 loadPrimitive**

将当前 `loadPrimitive<TIndexAccessor>` 拆为：

```cpp
bool extractPrimitiveMeshData(..., FCesiumPrimitiveMeshData& Out);
TUniquePtr<FStaticMeshRenderData>
buildLegacyRenderData(const FCesiumPrimitiveMeshData& Mesh, ...);
```

保持以下行为不变：

- 缺失法线时生成 Flat Normal；
- 需要时使用 MikkTSpace 生成 Tangent；
- Metadata/Feature ID UV 映射；
- Full Precision UV；
- Water Mask；
- Edge Visibility；
- Ray Tracing Legacy 初始化；
- Chaos Collision。

允许在公共提取阶段使用临时 `FStaticMeshVertexBuffers` 完成现有 Tangent
算法，然后复制到公共数组；不得创建完整 Legacy RenderData，除非最终选择
Legacy。

- [ ] **Step 5：运行原有 glTF/元数据测试**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -unattended -nop4 -NullRHI `
  -ExecCmds='Automation RunTests Cesium.Unit.Gltf;Automation RunTests Cesium.Unit.Metadata;Quit' `
  -TestExit='Automation Test Queue Empty' -log
```

Expected: 相关现有测试全部通过。

- [ ] **Step 6：提交公共网格重构**

```powershell
git add -- `
  Source/CesiumRuntime/Private/RuntimeNanite/CesiumRuntimeNaniteTypes.h `
  Source/CesiumRuntime/Private/CesiumGltfComponent.cpp `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "refactor: extract common Cesium primitive mesh data"
```

---

### Task 6：接入 Nanite/Legacy 工作线程分流

**Files:**

- Modify: `Source/CesiumRuntime/Private/LoadGltfResult.h`
- Modify: `Source/CesiumRuntime/Private/CesiumGltfComponent.cpp`
- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：编写路径选择失败测试**

增加：

```cpp
enum class ECesiumPrimitiveRenderPath : uint8 {
  Legacy,
  RuntimeNanite
};
```

测试：

- 配置关闭 → Legacy；
- 低于阈值 → Legacy；
- Translucent → Legacy；
- `TRIANGLE_STRIP` → Legacy；
- Builder 被注入为失败 → Legacy 且 `pRenderData` 有效；
- Eligible + Builder 成功 → RuntimeNanite。

- [ ] **Step 2：运行测试验证 RED**

Expected: FAIL，因为路径标记和分流不存在。

- [ ] **Step 3：在 LoadedPrimitiveResult 记录路径和原因**

增加：

```cpp
ECesiumPrimitiveRenderPath RenderPath =
    ECesiumPrimitiveRenderPath::Legacy;
ECesiumRuntimeNaniteFallbackReason NaniteFallbackReason =
    ECesiumRuntimeNaniteFallbackReason::DisabledByActor;
```

- [ ] **Step 4：接入资格策略和 Builder**

在公共网格提取完成后：

1. 根据 glTF `material.alphaMode` 判断 Translucent。
2. 根据 `CreateModelOptions`、CVar、`UseNanite(GMaxRHIShaderPlatform)`、
   Triangle Count 和 UV 数量创建资格输入。
3. Eligible 时调用 Runtime Nanite Builder。
4. Builder 返回空时记录 `BuilderFailed` 并调用 Legacy Builder。
5. 不 Eligible 时直接调用 Legacy Builder。
6. 仅在 `LogFallbacks` 打开时输出回退原因，避免默认刷屏。

- [ ] **Step 5：保持碰撞独立**

无论最终渲染路径如何，只要 `CreatePhysicsMeshes` 开启，就使用公共位置和索引
构建现有 Chaos Triangle Mesh。禁止从 Nanite Cluster 索引反向构建碰撞。

- [ ] **Step 6：运行路径选择和回退测试**

Expected: Nanite 成功、资格回退和 Builder 失败回退全部通过。

- [ ] **Step 7：提交工作线程分流**

```powershell
git add -- `
  Source/CesiumRuntime/Private/LoadGltfResult.h `
  Source/CesiumRuntime/Private/CesiumGltfComponent.cpp `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "feat: build eligible Cesium primitives with Nanite"
```

---

### Task 7：配置游戏线程 UStaticMesh Nanite 资源

**Files:**

- Modify: `Source/CesiumRuntime/Private/CesiumGltfComponent.cpp`
- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`

- [ ] **Step 1：增加临时 UStaticMesh 测试**

在游戏线程创建 Builder 产物对应的 `UStaticMesh`，验证：

```cpp
TestTrue(TEXT("Static mesh has Nanite data"), pMesh->HasValidNaniteData());
TestFalse(TEXT("Runtime Nanite disables ray tracing"), pMesh->bSupportRayTracing);
TestTrue(
    TEXT("Nanite setting is enabled"),
    pMesh->GetNaniteSettings().bEnabled);
```

- [ ] **Step 2：运行测试验证 RED**

Expected: FAIL，因为 `createStaticMesh` 尚未根据 RenderPath 配置 Nanite。

- [ ] **Step 3：扩展 createStaticMesh**

传入 `ECesiumPrimitiveRenderPath`：

```cpp
if (RenderPath == ECesiumPrimitiveRenderPath::RuntimeNanite) {
  FMeshNaniteSettings Settings = pStaticMesh->GetNaniteSettings();
  Settings.bEnabled = true;
  pStaticMesh->SetNaniteSettings(Settings);
  pStaticMesh->bSupportRayTracing = false;
} else {
  // 保留现有按 primitive mode 设置 ray tracing 的逻辑。
}
```

保持以下顺序：

1. 创建 transient `UStaticMesh`；
2. 设置 Mesh Component；
3. 设置 Flags 和 `NeverStream`；
4. 设置 RenderData；
5. 设置 Nanite Settings/Ray Tracing；
6. 添加材质；
7. `InitResources()`；
8. 设置 Bounds、Body Setup 和 Collision；
9. 注册组件。

- [ ] **Step 4：验证资源销毁**

创建和销毁多个 Runtime Nanite 图元，执行至少一次 GC：

```cpp
CollectGarbage(RF_NoFlags);
```

Expected: 无崩溃、无失效资源访问，组件和临时 Mesh 可回收。

- [ ] **Step 5：运行 UStaticMesh 测试验证 GREEN**

Expected: `HasValidNaniteData()` 为 true，Ray Tracing 为 false。

- [ ] **Step 6：提交游戏线程集成**

```powershell
git add -- `
  Source/CesiumRuntime/Private/CesiumGltfComponent.cpp `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "feat: initialize runtime Nanite static meshes"
```

---

### Task 8：验证 Raster Overlay、元数据拾取和碰撞

**Files:**

- Modify: `Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp`
- Modify only if tests expose a defect:
  `Source/CesiumRuntime/Private/CesiumGltfComponent.cpp`
  `Source/CesiumRuntime/Private/RuntimeNanite/*`

- [ ] **Step 1：增加多 UV Builder 测试**

创建包含：

- 材质 UV；
- Raster Overlay UV；
- Feature ID/Property Texture UV；

的输入，验证每组 UV 都被 Builder 接收，且不超过 `NANITE_MAX_UVS` 时成功。

- [ ] **Step 2：增加元数据命中测试**

使用现有 Chaos 碰撞结果执行 Line Trace，再调用：

```cpp
UCesiumMetadataPickingBlueprintLibrary::GetPropertyTableValuesFromHit(...)
```

Expected: Runtime Nanite 与 Legacy 返回相同 Feature 属性。

- [ ] **Step 3：增加碰撞测试**

在 Runtime Nanite 开启和关闭两种情况下对相同 Tile 执行 Line Trace。

Expected: 两种路径都命中，Face Index 对现有碰撞/元数据计算有效。

- [ ] **Step 4：运行集成测试**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -unattended -nop4 -NullRHI `
  -ExecCmds='Automation RunTests Cesium.Unit.RuntimeNanite;Quit' `
  -TestExit='Automation Test Queue Empty' -log
```

Expected: RuntimeNanite 测试组全部成功，退出码 0。

- [ ] **Step 5：提交兼容性测试**

```powershell
git add -- Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "test: cover runtime Nanite Cesium integrations"
```

---

### Task 9：UE 5.7 构建与真实 Tileset 验证

**Files:**

- Verify: all Runtime Nanite files and integration changes
- Create test notes only if requested:
  `docs/cesium-runtime-nanite-validation.md`

- [ ] **Step 1：准备隔离 HostProject**

使用：

```text
C:\tmp\cesium-runtime-nanite-host\HostProject.uproject
```

将当前 worktree 插件同步到：

```text
C:\tmp\cesium-runtime-nanite-host\Plugins\CesiumForUnreal
```

不得从主工作树复制未提交的 Cesium3DTileset、工具链、子模块或 build 修改。

- [ ] **Step 2：编译 UE 5.7 Win64**

Run:

```powershell
& 'D:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  UnrealEditor Win64 Development `
  -Project='C:\tmp\cesium-runtime-nanite-host\HostProject.uproject' `
  -WaitMutex -NoHotReloadFromIDE
```

Expected:

- UHT 成功；
- `UnrealEditor-CesiumRuntime.dll` 编译及链接成功；
- 退出码 0。

- [ ] **Step 3：运行完整 RuntimeNanite 自动化测试**

Expected: `Cesium.Unit.RuntimeNanite` 全部通过，退出码 0。

- [ ] **Step 4：验证 b3dm 倾斜摄影**

用同一相机路径分别关闭/开启 Runtime Nanite，检查：

- Tile 正常加载；
- Opaque/Masked 正常渲染；
- Translucent 自动回退；
- Raster Overlay 正常；
- 元数据拾取正常；
- Tile LOD 切换无缺块；
- 原点重定位无偏移。

- [ ] **Step 5：验证 Quantized Mesh 地形和直接 glTF**

因为首版不区分来源格式，验证：

- 高于阈值的地形三角形图元可进入 Runtime Nanite；
- 小 Tile 按阈值回退；
- 直接 glTF/GLB 行为一致；
- 水体/法线贴图若存在视觉问题，通过诊断回退路径记录。

- [ ] **Step 6：执行加载卸载压力测试**

反复移动相机触发 Tile 加载和卸载，并至少执行一次 Tileset 重建。

Expected:

- 无崩溃；
- 无持续增长的 Nanite Resource/Page 数量；
- 无悬空 `UStaticMesh`；
- Builder 失败不会导致 Tile 缺失。

- [ ] **Step 7：记录性能对比**

在固定镜头路径记录：

- GPU Frame Time；
- Render Thread Time；
- Game Thread Time；
- Draw Calls；
- Visible Nanite Clusters；
- Tile 首次可见延迟；
- Builder 时间；
- 系统内存和显存峰值；
- 平均 FPS 与 1% Low。

- [ ] **Step 8：执行最终仓库检查**

Run:

```powershell
git diff --check
git status --short
git diff --name-only origin/main...HEAD
git submodule status extern/cesium-native
```

Expected:

- 无 whitespace error；
- `extern/cesium-native` 指针未修改；
- 无 build 目录；
- 无 Linux 工具链和 vcpkg triplet；
- 仅包含 Runtime Nanite 设计、计划、源码和测试。

- [ ] **Step 9：提交最终修正**

```powershell
git add -- `
  Source/CesiumRuntime/CesiumRuntime.Build.cs `
  Source/CesiumRuntime/Public/Cesium3DTileset.h `
  Source/CesiumRuntime/Private/Cesium3DTileset.cpp `
  Source/CesiumRuntime/Private/CreateGltfOptions.h `
  Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp `
  Source/CesiumRuntime/Private/LoadGltfResult.h `
  Source/CesiumRuntime/Private/CesiumGltfComponent.cpp `
  Source/CesiumRuntime/Private/RuntimeNanite `
  Source/CesiumRuntime/Private/Tests/CesiumRuntimeNanite.spec.cpp
git commit -m "fix: complete runtime Nanite validation"
```

若没有修正，不创建空提交。

---

## 实施注意事项

- 实施时先使用 `@superpowers:test-driven-development`。
- 遇到编译、渲染或资源生命周期异常时使用
  `@superpowers:systematic-debugging`。
- 宣称完成前必须使用 `@superpowers:verification-before-completion`。
- VoxelCore 代码只能从固定提交
  `d3910625a3bf295f60b52c84d13e43177991198b` 移植，避免实现过程中漂移。
- 首版优先保证自动回退和功能正确，再进行 Cluster 编码性能优化。
- 不因 Nanite 集成顺手修改 Cesium3DTileset 迁移或其他无关工作。
