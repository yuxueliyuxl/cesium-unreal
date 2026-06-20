# Cesium 运行时 Nanite 设计

## 目标

为 Cesium for Unreal 生成的、符合条件的三角形图元增加可选的运行时
Nanite 渲染能力。首版不区分 b3dm、地形、直接 glTF、i3dm 或其他来源格式，
所有最终转换为 `CesiumGltf::Model` 的合格图元均可进入 Nanite 路径。

该功能默认关闭。当平台不支持 Nanite、图元不符合条件或构建失败时，
必须自动回退到现有静态网格渲染路径。

## 范围

本次实现包括：

- 仅移植 VoxelCore 中运行时 Nanite Builder 所必需的代码。
- 保留现有 `UCesiumGltfComponent` 和
  `UCesiumGltfPrimitiveComponent` 组件层级。
- 保留 Cesium Tile 选择、流式加载、坐标变换、Raster Overlay、元数据、
  材质、碰撞和资源生命周期。
- 首先支持 Unreal Engine 5.7。
- 在运行时构建 Nanite 资源，不创建持久化资产。

本次实现不包括：

- 引入完整 VoxelCore 插件。
- 在首版按内容来源格式进行过滤。
- 使用 Nanite 替代 Cesium Tile LOD。
- 实现离线 Nanite 资产烘焙。
- 为运行时 Nanite 网格实现硬件光线追踪。
- 实现完整的离线 Nanite 多层简化层级。

## 版权与许可

移植的 Builder 和编码代码来源于采用 MIT 许可证的 VoxelCore。移植后的
源码文件必须保留 Voxel Plugin 版权声明和 MIT 许可证归属说明。

## 架构

### 运行时配置

在 `ACesium3DTileset` 中增加：

```cpp
bool EnableRuntimeNanite = false;
int32 RuntimeNaniteMinimumTriangleCount = 2000;
```

两个属性均可在编辑器中配置，并支持 Blueprint 访问。修改任一属性时销毁并
重建 Native Tileset，确保已经加载和随后加载的 Tile 使用一致的渲染路径。

增加用于开发调试的控制台变量：

```text
cesium.RuntimeNanite.Enable
cesium.RuntimeNanite.MinTriangles
cesium.RuntimeNanite.LogFallbacks
```

Actor 属性和全局启用 CVar 必须同时允许，Nanite 才能生效。有效的最小三角形
数量取 Actor 配置与非负全局覆盖值中的较大者；也可约定特定 CVar 值表示沿用
Actor 配置。

### 移植的 Builder

创建隔离的 Cesium 自有文件，例如：

- `CesiumRuntimeNaniteBuilder.h/.cpp`
- `CesiumRuntimeNaniteEncoding.h/.cpp`

Builder 接收：

- 三角形列表索引；
- 顶点位置；
- 法线；
- 可选顶点色；
- 零组或多组纹理坐标。

Builder 生成包含以下内容的 `FStaticMeshRenderData`：

- `Nanite::FResources`；
- Cluster 层级数据；
- Root Page 数据；
- Page Streaming 状态；
- Bounds；
- `UStaticMesh` 所要求的最小后备 LOD 数据。

移植代码可以使用 VoxelCore 实现所依赖的 Unreal Engine Renderer
Private/Internal API。Cesium Runtime 模块的构建规则只增加必需的私有包含路径
和模块依赖。

### 图元渲染路径选择

增加：

```cpp
enum class ECesiumPrimitiveRenderPath : uint8 {
  Legacy,
  RuntimeNanite
};
```

将选定路径保存在图元加载结果中，供诊断和游戏线程资源设置使用。

glTF Accessor 通过验证并转换为 Unreal 坐标体系的网格数据后，仅在满足以下
全部条件时选择 Nanite：

- 当前 Tileset 和全局配置均启用了运行时 Nanite。
- 当前 RHI 和 Shader Platform 支持 Nanite。
- 图元模式为 `TRIANGLES`。
- 材质不是 Translucent。
- 顶点位置、索引和法线有效。
- 三角形数量达到有效阈值。
- UV 组数量不超过 Nanite 支持的上限。

Opaque 和 Masked 材质可进入 Nanite 路径。首版中，Point、Line、
Triangle Strip 和 Triangle Fan 使用 Legacy 路径。

### 数据流与线程

加载流程如下：

```text
glTF 图元
  -> 验证 Accessor 和材质
  -> 提取通用位置 / 索引 / 法线 / 顶点色 / UV
  -> 判断运行时 Nanite 资格
     -> 符合条件：尝试 Runtime Nanite Builder
        -> 成功：保留 Nanite RenderData
        -> 失败：构建 Legacy RenderData
     -> 不符合条件：构建 Legacy RenderData
  -> 游戏线程创建临时 UStaticMesh
  -> 安装 RenderData 和材质
  -> 初始化渲染资源、碰撞、元数据及组件状态
```

Nanite Cluster 和 Page 构建在 Cesium 现有工作线程加载阶段执行。
`UStaticMesh` 修改、Nanite 启用标记、材质赋值以及 `InitResources()` 仍在
游戏线程执行。

不得预先同时构建 Nanite 和 Legacy 两套 RenderData。Nanite Builder 失败后，
再利用保留的通用网格数据构建 Legacy RenderData，以降低成功路径的 CPU 时间
和峰值内存。

通用网格提取与 Legacy RenderData 构建应适当解耦，使 Nanite 构建失败后无需
重新读取 glTF Accessor。

## 与现有 Cesium 行为的集成

### 材质

继续通过现有材质流程创建 Cesium 动态材质实例。当前每个 glTF Primitive
已经对应一个独立的临时 `UStaticMesh`，与移植 Builder 首版仅支持单 Section
的限制相符。

Translucent 材质始终走 Legacy 路径，Masked 材质允许使用 Nanite。

首版 Builder 不编码切线。使用法线贴图的图元暂时允许进入 Nanite 路径，但
必须提供诊断 CVar 或临时强制回退方式，用于对比光照结果。如果验证发现明显的
切线回归，则在完成切线编码前，将使用法线贴图的图元加入自动回退条件。

### Raster Overlay

Cesium Raster Overlay 所需的全部 UV 组都必须传入 Nanite Builder。现有材质
参数绑定和 Raster Tile 附加逻辑保持不变。

如果所需 Overlay 坐标超过 Nanite UV 数量限制，则使用 Legacy 路径。

### 元数据与拾取

Feature ID 和 Property Texture 继续由现有 `CesiumPrimitiveData`、元数据纹理
和材质参数管理。

实现必须验证 Nanite 渲染是否保留命中拾取所需要的顶点和三角形对应关系。
如果 Nanite 渲染路径不能为现有拾取操作提供可靠的 Face Index，则该操作必须
使用现有碰撞网格的结果，不能假设 Nanite Cluster 中的三角形索引与 glTF
索引直接对应。

### 碰撞

启用 `CreatePhysicsMeshes` 时，继续构建现有 Chaos 碰撞网格。Nanite
RenderData 不替代碰撞数据。碰撞创建和 `ECollisionEnabled` 状态仍由现有
Cesium Tile 生命周期控制。

### 坐标变换与地理参考

继续使用 `CesiumPrimitiveData::highPrecisionNodeTransform` 和
`UCesiumGltfPrimitiveComponent::UpdateTransformFromCesium`。Nanite 顶点位置
采用与当前 Legacy RenderData 相同的局部顶点坐标约定。

原点重定位、Georeference 变化、glTF Up Axis 转换和 `CESIUM_RTC` 应用，在
两种渲染路径中的行为必须一致。

### Cesium 与 Nanite 的 LOD 职责

Cesium 继续负责：

- 网络 Tile 流式加载；
- Tile 级 SSE 选择；
- Tile 级可见性；
- Tile 缓存与卸载。

Nanite 负责已加载 Tile 内部的 Cluster 级渲染与剔除。移植的 Builder 不要求
生成完整的离线 Nanite 简化层级，因此 Cesium 仍然是主要的几何 LOD 系统。

### 光线追踪

运行时 Nanite 网格设置 `bSupportRayTracing = false`，与参考的 VoxelCore
Builder 行为一致。需要传统硬件光线追踪的项目可以关闭运行时 Nanite。
增加光线追踪后备代理不在本次范围内。

## 失败处理

Builder 错误不得导致 Tile 消失。任何构建失败都应记录可选诊断原因，并调用
现有 Legacy Builder。

回退原因包括：

- 功能未启用；
- 平台或 RHI 不支持；
- 图元模式不支持；
- Translucent 材质；
- 三角形数量不足；
- 顶点位置、索引或法线缺失或无效；
- UV 组过多；
- 超出 Nanite 编码限制；
- Builder 构建失败；
- 游戏线程资源初始化前验证失败。

预期内的资格不满足不应记录为 Error。Builder 不变量失败或意外资源失败应记录
为 Warning，并在必要时限制日志频率。

如果 Nanite RenderData 已经在游戏线程替换网格资源后才失败，实现必须先释放
不完整资源，再应用 Legacy RenderData。应尽量在工作线程完成所有可预见的验证，
使该恢复路径只处理异常情况。

## 资源生命周期

运行时 Nanite 资源由现有 Cesium 图元组件对应的临时 `UStaticMesh` 持有。
现有 Tile 卸载流程继续销毁 Body Setup、Static Mesh、材质、图元组件和父级
glTF 组件。

必须验证反复加载、卸载和重建 Tileset 后，不会遗留已经销毁的临时网格所注册的
Nanite Page 资源。

## 测试

### 单元测试与自动化测试

覆盖以下行为：

- 运行时 Nanite 默认关闭。
- 最小三角形阈值默认值为 2000。
- Actor Setter 仅在值发生变化时重建 Tileset。
- 功能关闭时选择 Legacy。
- 小型网格选择 Legacy。
- Point、Line、Strip、Fan 和 Translucent 图元选择 Legacy。
- 符合条件的 Opaque 和 Masked 三角形网格选择 Runtime Nanite。
- UV 组数量过多时选择 Legacy。
- Builder 失败时选择 Legacy，并仍能生成可渲染数据。
- 成功构建的 RenderData 包含有效 `NaniteResourcesPtr`。
- 运行时 Nanite 网格关闭传统 Ray Tracing。

### UE 5.7 集成测试

测试以下内容：

- b3dm 倾斜摄影；
- Quantized Mesh 地形；
- 直接 glTF/GLB Tile 内容；
- Raster Overlay；
- b3dm Batch Table 元数据拾取；
- 碰撞与 Line Trace；
- Georeference 原点重定位；
- Tile LOD 过渡；
- 反复加载、卸载及重建 Tileset；
- Opaque、Masked、Translucent 和使用法线贴图的材质。

### 性能验证

使用相同相机路径和 Tileset 配置，对比 Legacy 与 Runtime Nanite：

- GPU Frame Time；
- Render Thread Time；
- Game Thread Time；
- Draw Call；
- 可见 Nanite Cluster；
- Tile 首次可见延迟；
- Nanite 工作线程构建耗时；
- 系统内存峰值；
- GPU 显存峰值；
- 平均 FPS 和 1% Low FPS。

原型验收条件：

- 不会因 Nanite 不受支持或构建失败而丢失测试内容。
- Raster Overlay、元数据拾取、碰撞和坐标变换无功能回归。
- 反复卸载 Tile 不发生资源泄漏或崩溃。
- 至少一个有代表性的高密度场景呈现可测量的 GPU 或 Render Thread 收益。
- 记录新增的 Tile 首次可见延迟和内存开销，供项目级调优。

## 实现边界

Nanite 工作必须与当前尚未提交的 Cesium3DTileset 迁移、Linux 工具链、
cesium-native 子模块和 build 目录修改相互独立。集成时不得意外提交这些无关
修改。
