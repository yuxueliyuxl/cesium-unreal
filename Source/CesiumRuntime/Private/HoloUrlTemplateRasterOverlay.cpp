// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#include "HoloUrlTemplateRasterOverlay.h"

#include "CesiumHoloveser/UrlTemplateRasterOverlay.h"

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
UHoloUrlTemplateRasterOverlay::CreateOverlay(
    const CesiumRasterOverlays::RasterOverlayOptions& options) {
  if (this->BaseUrl.IsEmpty()) {
    return nullptr;
  }

  CesiumHoloveser::UrlTemplateRasterOverlayOptions urlOptions;
  if (this->MaximumLevel > this->MinimumLevel) {
    urlOptions.minimumLevel = this->MinimumLevel;
    urlOptions.maximumLevel = this->MaximumLevel;
  }

#if WITH_EDITOR
  urlOptions.debug = this->bDebug;
#endif

  TArray<FString> subdomains;
  this->Subdoains.ParseIntoArray(subdomains, TEXT(","), true);
  for (FString& subdomain : subdomains) {
    subdomain.TrimStartAndEndInline();
    if (!subdomain.IsEmpty()) {
      urlOptions.subdomains.emplace_back(TCHAR_TO_UTF8(*subdomain));
    }
  }

  urlOptions.layers =
      this->Projection == EProjection::WGS84 ? "WGS84" : "GCJ02";
  urlOptions.tileWidth = this->TileWidth;
  urlOptions.tileHeight = this->TileHeight;

  return std::make_unique<CesiumHoloveser::UrlTemplateRasterOverlay>(
      TCHAR_TO_UTF8(*this->MaterialLayerKey),
      TCHAR_TO_UTF8(*this->BaseUrl),
      std::vector<CesiumAsync::IAssetAccessor::THeader>(),
      urlOptions,
      options);
}
