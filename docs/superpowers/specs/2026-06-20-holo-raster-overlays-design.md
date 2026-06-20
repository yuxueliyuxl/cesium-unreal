# Holo Raster Overlays Migration Design

## Goal

Migrate the three Unreal raster overlay components from the legacy plugin into
the current `cesium-unreal` repository:

- `UHoloIonRasterOverlay`
- `UHoloUrlTemplateRasterOverlay`
- `UHoloWebMapTileServiceRasterOverlay`

The migration preserves existing Blueprint class names, serialized property
names, map-type behavior, and the connection to the already-migrated
`CesiumHoloveser` native library.

## Scope

Add these files:

- `Source/CesiumRuntime/Public/HoloIonRasterOverlay.h`
- `Source/CesiumRuntime/Private/HoloIonRasterOverlay.cpp`
- `Source/CesiumRuntime/Public/HoloUrlTemplateRasterOverlay.h`
- `Source/CesiumRuntime/Private/HoloUrlTemplateRasterOverlay.cpp`
- `Source/CesiumRuntime/Public/HoloWebMapTileServiceRasterOverlay.h`
- `Source/CesiumRuntime/Private/HoloWebMapTileServiceRasterOverlay.cpp`

The official Cesium ion, URL-template, and WMTS overlay classes remain
unchanged.

## Component behavior

### Holo ion

Preserve `HoloVeserMapsType` and its legacy asset-ID mapping. The component
selects the configured Holoveser ion server and access token, normalizes the API
URL, and creates `CesiumHoloveser::HoloIonRasterOverlay`.

Invalid asset IDs or empty API URLs return no native overlay. `PostLoad`
preserves backward-compatible ion-server migration. The editor troubleshooting
action continues to broadcast through the existing Cesium delegate.

### Holo URL template

Preserve the existing URL, WGS84/GCJ02 selection, tile dimensions, zoom levels,
debug flag, and comma-separated subdomain configuration. Empty subdomain
entries are ignored. The component creates
`CesiumHoloveser::UrlTemplateRasterOverlay`.

The legacy public property name `Subdoains` remains unchanged to preserve
serialized assets and Blueprint compatibility.

### Holo WMTS

Preserve the base URL, provider key, optional zoom range, and debug flag. The
component creates `CesiumHoloveser::WebMapTileServiceRasterOverlay`.

An empty base URL returns no native overlay. Explicit zoom levels are applied
only when `bSpecifyZoomLevels` is true and the maximum is greater than the
minimum.

## Compatibility adaptations

- Use the current `UCesiumRasterOverlay::CreateOverlay` signature.
- Pass current `RasterOverlayOptions` through to native constructors so the
  selected ellipsoid is retained.
- Use current `UCesiumIonServer` APIs and current custom-version handling.
- Include only headers needed by each implementation.
- Rely on the existing linked `CesiumHoloveser` native library; no new Unreal
  module dependency is expected.

## Testing and verification

Add Unreal automation coverage for:

- default property values;
- Holo map-type to asset-ID mapping;
- construction of all three component classes;
- invalid or empty configuration returning no native overlay;
- parsing comma-separated URL-template subdomains.

Follow a red-green cycle: add tests before production files and confirm that
the initial build fails because the classes do not exist. Then implement the
components and verify:

1. Unreal Header Tool succeeds under UE 5.7;
2. the Win64 UnrealEditor target links successfully;
3. the new focused automation tests pass;
4. `git diff --check` reports no whitespace errors.

## Non-goals

- Renaming legacy Blueprint properties or enum values.
- Replacing the Holoveser native implementations with official Cesium overlay
  classes.
- Refactoring unrelated raster overlay code.
- Changing service URLs, credentials, or map-type IDs.
