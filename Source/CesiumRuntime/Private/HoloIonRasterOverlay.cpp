// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#include "HoloIonRasterOverlay.h"

#include "CesiumActors.h"
#include "CesiumCustomVersion.h"
#include "CesiumHoloveser/HoloIonRasterOverlay.h"
#include "CesiumIonServer.h"
#include "CesiumRuntime.h"

void UHoloIonRasterOverlay::HoloveserMapToken() {
  OnCesiumRasterOverlayIonTroubleshooting.Broadcast(this);
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
UHoloIonRasterOverlay::CreateOverlay(
    const CesiumRasterOverlays::RasterOverlayOptions& options) {
  switch (this->HoloveserIonMapType) {
  case HoloVeserMapsType::gaode_img:
    this->HoloveserIonAssetID = 1;
    break;
  case HoloVeserMapsType::gaode_vec:
    this->HoloveserIonAssetID = 11;
    break;
  case HoloVeserMapsType::google_img_marker:
    this->HoloveserIonAssetID = 21;
    break;
  case HoloVeserMapsType::google_img:
    this->HoloveserIonAssetID = 22;
    break;
  case HoloVeserMapsType::google_vec_marker:
    this->HoloveserIonAssetID = 23;
    break;
  case HoloVeserMapsType::google_ter:
    this->HoloveserIonAssetID = 24;
    break;
  case HoloVeserMapsType::google_road_marker:
    this->HoloveserIonAssetID = 25;
    break;
  case HoloVeserMapsType::tiandidu_img_c:
    this->HoloveserIonAssetID = 3;
    break;
  case HoloVeserMapsType::tiandidu_cia_c:
    this->HoloveserIonAssetID = 31;
    break;
  case HoloVeserMapsType::tiandidu_vec_c:
    this->HoloveserIonAssetID = 32;
    break;
  case HoloVeserMapsType::tiandidu_cva_c:
    this->HoloveserIonAssetID = 33;
    break;
  case HoloVeserMapsType::tiandidu_ter_c:
    this->HoloveserIonAssetID = 34;
    break;
  case HoloVeserMapsType::tiandidu_ibo_c:
    this->HoloveserIonAssetID = 35;
    break;
  case HoloVeserMapsType::tiandidu_img_w:
    this->HoloveserIonAssetID = 7;
    break;
  case HoloVeserMapsType::tiandidu_cia_w:
    this->HoloveserIonAssetID = 71;
    break;
  case HoloVeserMapsType::tiandidu_vec_w:
    this->HoloveserIonAssetID = 72;
    break;
  case HoloVeserMapsType::tiandidu_cva_w:
    this->HoloveserIonAssetID = 73;
    break;
  case HoloVeserMapsType::tiandidu_ter_w:
    this->HoloveserIonAssetID = 74;
    break;
  case HoloVeserMapsType::tiandidu_ibo_w:
    this->HoloveserIonAssetID = 75;
    break;
  case HoloVeserMapsType::tiandidu_lc_land:
    this->HoloveserIonAssetID = 76;
    break;
  case HoloVeserMapsType::tiandidu_lc_terrain_rgb:
    this->HoloveserIonAssetID = 77;
    break;
  case HoloVeserMapsType::tiandidu_js_vec_blue:
    this->HoloveserIonAssetID = 6;
    break;
  case HoloVeserMapsType::tiandidu_js_vec_black:
    this->HoloveserIonAssetID = 61;
    break;
  case HoloVeserMapsType::tiandidu_js_vec_grey:
    this->HoloveserIonAssetID = 62;
    break;
  case HoloVeserMapsType::tiandidu_js_vec:
    this->HoloveserIonAssetID = 63;
    break;
  case HoloVeserMapsType::geovisearth_img:
    this->HoloveserIonAssetID = 4;
    break;
  case HoloVeserMapsType::geovisearth_vec:
    this->HoloveserIonAssetID = 41;
    break;
  case HoloVeserMapsType::geovisearth_ter:
    this->HoloveserIonAssetID = 42;
    break;
  case HoloVeserMapsType::geovisearth_cia:
    this->HoloveserIonAssetID = 43;
    break;
  case HoloVeserMapsType::mapbox_img:
    this->HoloveserIonAssetID = 5;
    break;
  case HoloVeserMapsType::mapbox_vec:
    this->HoloveserIonAssetID = 51;
    break;
  case HoloVeserMapsType::mapbox_lanuse:
    this->HoloveserIonAssetID = 52;
    break;
  case HoloVeserMapsType::mapbox_color:
    this->HoloveserIonAssetID = 53;
    break;
  case HoloVeserMapsType::Customize:
    this->HoloveserIonAssetID = 99;
    break;
  }

  if (this->HoloveserIonAssetID <= 0) {
    return nullptr;
  }

  if (!IsValid(this->HoloveserIonServer)) {
    this->HoloveserIonServer = UCesiumIonServer::GetServerForNewObjects();
  }
  if (!IsValid(this->HoloveserIonServer)) {
    return nullptr;
  }

  const FString token = this->HoloveserIonAccessToken.IsEmpty()
                            ? this->HoloveserIonServer->DefaultIonAccessToken
                            : this->HoloveserIonAccessToken;

#if WITH_EDITOR
  this->HoloveserIonServer->ResolveApiUrl();
#endif

  std::string apiUrl = TCHAR_TO_UTF8(*this->HoloveserIonServer->ApiUrl);
  if (apiUrl.empty()) {
    return nullptr;
  }
  if (apiUrl.back() != '/') {
    apiUrl += '/';
  }

  return std::make_unique<CesiumHoloveser::HoloIonRasterOverlay>(
      TCHAR_TO_UTF8(*this->MaterialLayerKey),
      this->HoloveserIonAssetID,
      TCHAR_TO_UTF8(*token),
      this->bDebug,
      options,
      apiUrl);
}

void UHoloIonRasterOverlay::PostLoad() {
  Super::PostLoad();

  if (CesiumActors::shouldValidateFlags(this)) {
    CesiumActors::validateActorComponentFlags(this);
  }

#if WITH_EDITOR
  const int32 CesiumVersion =
      this->GetLinkerCustomVersion(FCesiumCustomVersion::GUID);

  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  if (CesiumVersion < FCesiumCustomVersion::CesiumIonServer) {
    this->HoloveserIonServer = UCesiumIonServer::GetBackwardCompatibleServer(
        this->IonAssetEndpointUrl_DEPRECATED);
  }
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}
