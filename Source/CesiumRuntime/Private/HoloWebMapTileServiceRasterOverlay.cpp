// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#include "HoloWebMapTileServiceRasterOverlay.h"

#include "CesiumHoloveser/WebMapTileServiceRasterOverlay.h"

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
UHoloWebMapTileServiceRasterOverlay::CreateOverlay(
    const CesiumRasterOverlays::RasterOverlayOptions& options) {
  if (this->BaseUrl.IsEmpty()) {
    return nullptr;
  }

  CesiumHoloveser::WebMapTileServiceRasterOverlayOptions wmtsOptions;
  if (this->bSpecifyZoomLevels && this->MaximumLevel > this->MinimumLevel) {
    wmtsOptions.minimumLevel = this->MinimumLevel;
    wmtsOptions.maximumLevel = this->MaximumLevel;
  }
  wmtsOptions.key = TCHAR_TO_UTF8(*this->Key);

#if WITH_EDITOR
  wmtsOptions.debug = this->bDebug;
#endif

  return std::make_unique<CesiumHoloveser::WebMapTileServiceRasterOverlay>(
      TCHAR_TO_UTF8(*this->MaterialLayerKey),
      TCHAR_TO_UTF8(*this->BaseUrl),
      std::vector<CesiumAsync::IAssetAccessor::THeader>(),
      wmtsOptions,
      options);
}
