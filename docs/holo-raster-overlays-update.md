# Holo Raster Overlays 更新说明

## 更新内容

本次新增并适配以下 Holoveser 栅格图层组件：

- `UHoloIonRasterOverlay`
- `UHoloUrlTemplateRasterOverlay`
- `UHoloWebMapTileServiceRasterOverlay`

组件继续调用 `cesium-native` 中的 `CesiumHoloveser` 实现，不修改或替换
Cesium 官方 Raster Overlay 组件。

## 功能说明

### Holo Ion Raster Overlay

- 保留原有 `HoloVeserMapsType` 地图类型枚举。
- 保留地图类型到 Holoveser Asset ID 的映射。
- 支持访问令牌、Ion Server 和调试开关。
- 保留旧资产的 Ion Server 兼容迁移逻辑。
- Ion API URL 为空或服务器无效时不创建 Native Overlay。

### Holo URL Template Raster Overlay

- 支持 WGS84 和 GCJ02。
- 支持瓦片尺寸、最小/最大层级及调试开关。
- 支持逗号分隔的子域名列表，并自动忽略空项。
- 为兼容已有蓝图和序列化资产，保留原属性名 `Subdoains`。
- URL 为空时不创建 Native Overlay。

### Holo WMTS Raster Overlay

- 支持服务 URL、服务 Key 和调试开关。
- 支持选择是否显式指定最小/最大层级。
- 仅当 `bSpecifyZoomLevels` 启用且最大层级大于最小层级时覆盖默认层级。
- URL 为空时不创建 Native Overlay。

## 兼容性

- 使用当前版本的 `UCesiumRasterOverlay::CreateOverlay` 接口。
- 将当前 `RasterOverlayOptions` 传递给 Native Overlay。
- 兼容 Unreal Engine 5.7 的 UHT 和 C++ 编译。
- 未修改 Cesium 官方 Ion、URL Template 和 WMTS Overlay 类。

## 验证结果

- UE 5.7 Unreal Header Tool：通过。
- UE 5.7 Win64 `UnrealEditor-CesiumRuntime.dll`：编译及链接通过。
- 自动化测试 `Cesium.Unit.RasterOverlays.HoloDefaults`：通过。
- 测试退出码：`0`。

编译过程中临时排除了与本次迁移无关的旧测试
`CesiumPropertyAttributeProperty.spec.cpp`。该文件仍使用旧版
`AccessorView` 构造接口，本次没有修改。

## 人工测试

隔离源码：

`E:\UnrealProjects\cesium-unreal\.worktrees\holo-raster-overlays`

UE 5.7 临时测试工程：

`C:\tmp\cesium-holo-overlay-host\HostProject.uproject`

建议分别添加三个组件，检查：

1. Holo Ion 各地图类型是否能够加载。
2. URL Template 的 WGS84、GCJ02 和子域名请求是否正确。
3. WMTS 的 Key、默认层级和显式层级配置是否生效。
4. 参数修改或组件重载后图层能否正常刷新。
