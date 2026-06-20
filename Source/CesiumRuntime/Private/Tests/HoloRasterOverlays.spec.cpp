// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#include "HoloIonRasterOverlay.h"
#include "HoloUrlTemplateRasterOverlay.h"
#include "HoloWebMapTileServiceRasterOverlay.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FHoloRasterOverlaysDefaults,
    "Cesium.Unit.RasterOverlays.HoloDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter);

bool FHoloRasterOverlaysDefaults::RunTest(const FString& Parameters) {
  UHoloIonRasterOverlay* pIon = NewObject<UHoloIonRasterOverlay>();
  TestNotNull(TEXT("Holo ion overlay can be created"), pIon);
  if (pIon) {
    TestEqual(
        TEXT("Holo ion defaults to Gaode imagery"),
        pIon->HoloveserIonMapType,
        HoloVeserMapsType::gaode_img);
    TestEqual(
        TEXT("Holo ion asset ID defaults to zero"),
        pIon->HoloveserIonAssetID,
        int64(0));
    TestFalse(TEXT("Holo ion debug defaults to false"), pIon->bDebug);
  }

  UHoloUrlTemplateRasterOverlay* pUrl =
      NewObject<UHoloUrlTemplateRasterOverlay>();
  TestNotNull(TEXT("Holo URL-template overlay can be created"), pUrl);
  if (pUrl) {
    TestEqual(
        TEXT("Holo URL-template defaults to WGS84"),
        pUrl->Projection,
        EProjection::WGS84);
    TestEqual(TEXT("URL tile width defaults to 256"), pUrl->TileWidth, 256);
    TestEqual(TEXT("URL tile height defaults to 256"), pUrl->TileHeight, 256);
    TestEqual(
        TEXT("URL minimum level defaults to zero"),
        pUrl->MinimumLevel,
        0);
    TestEqual(TEXT("URL maximum level defaults to 18"), pUrl->MaximumLevel, 18);
    TestFalse(TEXT("URL debug defaults to false"), pUrl->bDebug);
  }

  UHoloWebMapTileServiceRasterOverlay* pWmts =
      NewObject<UHoloWebMapTileServiceRasterOverlay>();
  TestNotNull(TEXT("Holo WMTS overlay can be created"), pWmts);
  if (pWmts) {
    TestFalse(
        TEXT("WMTS zoom levels are not explicitly specified by default"),
        pWmts->bSpecifyZoomLevels);
    TestEqual(
        TEXT("WMTS minimum level defaults to zero"),
        pWmts->MinimumLevel,
        0);
    TestEqual(
        TEXT("WMTS maximum level defaults to 18"),
        pWmts->MaximumLevel,
        18);
    TestFalse(TEXT("WMTS debug defaults to false"), pWmts->bDebug);
  }

  return true;
}
