# cesium-native 定制修改说明

本文档记录 `yuxueliyuxl/cesium-native` 相对官方版本的定制功能，供后续升级
cesium-native、迁移到新版本 Cesium for Unreal、排查编译问题时使用。

## 1. 仓库与版本

- cesium-native 仓库：`https://github.com/yuxueliyuxl/cesium-native`
- Cesium for Unreal 仓库：`https://github.com/yuxueliyuxl/cesium-unreal`
- native 子模块目录：`extern/cesium-native`
- 本次定制基线：`bfc2c574cd318ea8a744e137304b8febc4199fd6`
- 投影、栅格覆盖和地形提交：`53523e9290fa2584e31c0dfbb7426b61d38cc26a`
- 忽略 Tileset Transform 提交：`838a2b349ceb8a58482987f33deb24ec1c1516e6`

查看完整定制差异时，应优先检查上述两个 native 提交，避免把后续官方更新误认为
定制代码：

```powershell
git show 53523e929
git show 838a2b349
```

## 2. 定制功能概览

| 功能 | 配置/API | 默认行为 |
| --- | --- | --- |
| GCJ-02 Web Mercator 投影 | `WebMercatorProjection(Ellipsoid, "GCJ02")` | `"WGS84"` |
| 自定义栅格覆盖模块 | `CesiumHoloveser` | 按需创建 Overlay |
| 地形夸张 | `TilesetContentOptions::terrainExaggeration` | `1.0` |
| 地形多边形平滑/压平 | `TilesetContentOptions::TerrainSmoothingConfigs` | 空数组 |
| 忽略 tileset transform | `TilesetContentOptions::ignoreTransform` | `false` |

所有新增选项均提供兼容默认值。未显式启用时，应保持官方原有行为。

## 3. GCJ-02 投影

### 3.1 涉及文件

- `CesiumGeospatial/include/CesiumGeospatial/WebMercatorProjection.h`
- `CesiumGeospatial/src/WebMercatorProjection.cpp`
- `CesiumGeospatial/src/ProjectionConvert.h`
- `CesiumGeospatial/src/ProjectionConvert.cpp`
- `CesiumGeospatial/test/TestProjection.cpp`

### 3.2 API

`WebMercatorProjection` 构造函数增加投影类型参数：

```cpp
WebMercatorProjection(
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84,
    std::string projection = "WGS84") noexcept;
```

使用 GCJ-02：

```cpp
CesiumGeospatial::WebMercatorProjection projection(
    CesiumGeospatial::Ellipsoid::WGS84,
    "GCJ02");
```

### 3.3 数据流

- `project`：先将 WGS84 经纬度转换为 GCJ-02，再执行 Web Mercator 投影。
- `unproject`：先执行 Web Mercator 反投影，再将 GCJ-02 转换回 WGS84。
- 中国境外坐标由 `ProjectionConvert::out_of_china` 判断，境外不偏移。

### 3.4 当前实现注意事项

当前判断逻辑是：

```cpp
if (_projection != "WGS84") {
  // 执行 GCJ-02 转换
}
```

因此任何非 `"WGS84"` 字符串都会进入 GCJ-02 分支。后续维护时建议改为显式判断
`"GCJ02"`，并为非法值定义回退或错误处理。

## 4. CesiumHoloveser 模块

### 4.1 构建接入

根 `CMakeLists.txt` 增加：

```cmake
add_subdirectory(CesiumHoloveser)
```

`CesiumHoloveser/CMakeLists.txt` 创建并安装 `CesiumHoloveser` 静态库，依赖：

- `CesiumAsync`
- `CesiumGeospatial`
- `CesiumGeometry`
- `CesiumGltf`
- `CesiumGltfContent`
- `CesiumGltfReader`
- `CesiumRasterOverlays`
- `CesiumUtility`
- `tinyxml2`

### 4.2 主要组件

- `ArcGisMapServerRasterOverlay`
- `TiandituRasterOverlay`
- `UrlTemplateRasterOverlay`
- `WebMapTileServiceRasterOverlay`
- `HoloIonRasterOverlay`
- `HoloCreditSystem`
- HMAC-SHA256 支持

公共头文件位于：

```text
CesiumHoloveser/include/CesiumHoloveser/
```

实现位于：

```text
CesiumHoloveser/src/
```

### 4.3 维护注意事项

目录中保留了 `GCJ02Projection.rar`、`HoloIonRasterOverlay.rar` 等历史归档文件。
它们不参与 C++ 编译，但会进入 Git 和安装目录。后续确认不再需要后，可单独清理，
不要在官方版本迁移时将其误认为必要源码。

## 5. 地形夸张

### 5.1 配置入口

在 `Cesium3DTilesSelection/TilesetOptions.h` 的
`TilesetContentOptions` 中增加：

```cpp
double terrainExaggeration = 1.0;
```

调用链：

```text
TilesetContentOptions
  -> LayerJsonTerrainLoader::requestTileContent
  -> QuantizedMeshLoader::load
```

### 5.2 Loader API

`QuantizedMeshLoader::load` 增加参数：

```cpp
double terrainExaggeration = 1.0
```

### 5.3 处理逻辑

- Quantized Mesh 头部的 `MinimumHeight` 和 `MaximumHeight` 乘以夸张系数。
- 顶点高度根据调整后的高度范围解码。
- skirt 高度乘以 `abs(terrainExaggeration)`，避免负系数产生反向 skirt。

默认值为 `1.0`，不改变原始地形。

## 6. 地形平滑/多边形压平

### 6.1 配置结构

`CesiumGeospatial/CartographicPolygon.h` 增加：

```cpp
struct CESIUMGEOSPATIAL_API HoloTerrainSmoothingConfig {
  int StartLevel = 5;
  double Height = 0;
  CartographicPolygon Polygon{{}};
};
```

`TilesetContentOptions` 增加：

```cpp
std::vector<CesiumGeospatial::HoloTerrainSmoothingConfig>
    TerrainSmoothingConfigs;
```

`QuantizedMeshLoader::load` 增加：

```cpp
const std::vector<CesiumGeospatial::HoloTerrainSmoothingConfig>&
    terrainSmoothingConfigs = {}
```

### 6.2 处理逻辑

对 Quantized Mesh 的每个顶点：

1. 计算顶点经纬度。
2. 仅当 `tileID.level >= StartLevel` 时检查多边形。
3. 使用多边形三角形索引和
   `CesiumGeometry::IntersectionTests::pointInTriangle` 判断顶点是否位于区域内。
4. 命中后将该顶点高度直接设置为配置中的 `Height`。
5. 根据平滑后的高度重新计算 glTF 高度归一化值和包围高度范围。

### 6.3 当前实现注意事项

- 这是区域内固定高度压平，而不是边缘渐变式平滑。
- 多个配置同时命中时，后面的配置仍可覆盖前面的高度。
- 用于更新整体最小/最大高度范围的是最后一次记录到的命中配置。
- 多边形顶点和 Quantized Mesh 顶点使用弧度制经纬度。

未来如果需要柔和边缘，应新增过渡距离和插值逻辑，不应直接改变现有配置含义。

## 7. 忽略 tileset.json 的 transform

### 7.1 配置入口

`TilesetContentOptions` 中增加：

```cpp
bool ignoreTransform = false;
```

### 7.2 涉及文件

- `Cesium3DTilesSelection/include/Cesium3DTilesSelection/TilesetOptions.h`
- `Cesium3DTilesSelection/src/TilesetContentManager.cpp`
- `Cesium3DTilesSelection/src/TilesetJsonLoader.h`
- `Cesium3DTilesSelection/src/TilesetJsonLoader.cpp`
- `Cesium3DTilesSelection/test/TestTilesetJsonLoader.h`
- `Cesium3DTilesSelection/test/TestTilesetJsonLoader.cpp`

### 7.3 数据流

```text
TilesetContentOptions::ignoreTransform
  -> TilesetContentManager::createFromUrl
  -> TilesetJsonLoader::createLoader
  -> TilesetJsonLoader::_ignoreTransform
  -> parseTileJsonRecursively
```

启用后，JSON 中每个 tile 的 `transform` 被单位矩阵替代：

```cpp
const std::optional<glm::dmat4x4> transform =
    currentLoader.getIgnoreTransform()
        ? std::optional<glm::dmat4x4>(glm::dmat4x4(1.0))
        : JsonHelpers::getTransformProperty(tileJson, "transform");
```

仍然保留调用方传入的 `parentTransform`。因此该选项只忽略 tileset JSON 自身声明的
矩阵，不会清除程序在外层施加的变换。

### 7.4 外部嵌套 tileset

新版实现会将 `_ignoreTransform` 继续传给外部嵌套 tileset 的 loader，使整棵
tileset 树保持一致。

这与旧实现不同：旧实现只影响主 tileset，没有把该参数继续传给外部 tileset。

### 7.5 测试

`TestTilesetJsonLoader.cpp` 增加两类回归测试：

- 忽略根 tile 和子 tile 的 transform，并验证几何误差不再被缩放矩阵放大。
- 验证外部嵌套 `TilesetJsonLoader` 继承 `ignoreTransform=true`。

## 8. 完整文件清单

### 8.1 新增文件

```text
CesiumGeospatial/src/ProjectionConvert.cpp
CesiumGeospatial/src/ProjectionConvert.h
CesiumHoloveser/CMakeLists.txt
CesiumHoloveser/include/CesiumHoloveser/*
CesiumHoloveser/src/*
```

### 8.2 修改文件

```text
CMakeLists.txt
Cesium3DTilesSelection/include/Cesium3DTilesSelection/TilesetOptions.h
Cesium3DTilesSelection/src/LayerJsonTerrainLoader.cpp
Cesium3DTilesSelection/src/TilesetContentManager.cpp
Cesium3DTilesSelection/src/TilesetJsonLoader.cpp
Cesium3DTilesSelection/src/TilesetJsonLoader.h
Cesium3DTilesSelection/test/TestTilesetJsonLoader.cpp
Cesium3DTilesSelection/test/TestTilesetJsonLoader.h
CesiumGeospatial/include/CesiumGeospatial/CartographicPolygon.h
CesiumGeospatial/include/CesiumGeospatial/WebMercatorProjection.h
CesiumGeospatial/src/WebMercatorProjection.cpp
CesiumGeospatial/test/TestProjection.cpp
CesiumQuantizedMeshTerrain/include/CesiumQuantizedMeshTerrain/QuantizedMeshLoader.h
CesiumQuantizedMeshTerrain/src/QuantizedMeshLoader.cpp
```

## 9. Windows 编译

当前验证环境：

- Unreal Engine 5.7.4
- Visual Studio 2022
- MSVC 14.44
- x64 Release

在 `cesium-unreal/extern` 下配置：

```powershell
cmake -B build -S . `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -T "version=14.44" `
  -DUNREAL_ENGINE_ROOT="D:/Program Files/Epic Games/UE_5.7"
```

编译并安装：

```powershell
cmake --build build --config Release --target install --parallel
```

产物安装到：

```text
Source/ThirdParty/lib/Windows-AMD64-Release/
Source/ThirdParty/include/
```

本次完整 Windows Release 编译和 install 已通过。

## 10. 测试

ignoreTransform 使用独立测试构建：

```powershell
cmake -B build-ignore-transform-test -S cesium-native `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -T "version=14.44" `
  -DBUILD_TESTING=ON `
  -DCESIUM_TESTS_ENABLED=ON

cmake --build build-ignore-transform-test `
  --config Release `
  --target cesium-native-tests `
  --parallel
```

相关测试：

```powershell
.\build-ignore-transform-test\CesiumNativeTests\Release\cesium-native-tests.exe `
  --test-case="Test creating tileset json loader"

.\build-ignore-transform-test\CesiumNativeTests\Release\cesium-native-tests.exe `
  --test-case="Test loading individual tile of tileset json"
```

验证结果：

- 创建 loader：`221 / 221` assertions 通过。
- 外部 tileset 加载：`203 / 203` assertions 通过。

## 11. 后续同步官方版本的建议流程

1. 在私有 `cesium-native` 仓库中同步官方最新代码。
2. 创建专用迁移分支，不直接在 detached HEAD 上修改。
3. 优先尝试将两个定制提交 rebase/cherry-pick 到新基线。
4. 冲突时按本文档的数据流逐项迁移，不要直接覆盖官方新文件。
5. 首先迁移配置结构和公开 API，再迁移调用链和实现。
6. 编译 native 测试目标并运行投影、TilesetJsonLoader 相关测试。
7. 执行完整 Windows Release install。
8. 提交 native 修改。
9. 在 cesium-unreal 中更新 `extern/cesium-native` 子模块指针并单独提交。

建议保持功能提交相互独立，例如：

```text
Add GCJ02 projection support
Add CesiumHoloveser overlays
Add terrain exaggeration and smoothing
Add option to ignore tileset transforms
```

这样在官方接口变化时，可以单独定位、迁移或撤销某项功能。

## 12. Git 提交关系

```text
cesium-native
  53523e929  Add custom projections and terrain processing
  838a2b349  Add option to ignore tileset transforms
```

外层仓库只记录 native 子模块 commit。必须先确保 native commit 已经推送到
`yuxueliyuxl/cesium-native`，再推送对应的 cesium-unreal commit，否则其他机器
无法初始化该子模块版本。
