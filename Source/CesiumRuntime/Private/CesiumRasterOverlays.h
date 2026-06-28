// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#pragma once

constexpr size_t maximumOverlayTextureCoordinateIDs = 3;//zzt 原值为2，+1后为多一个图层投影

/**
 * @brief Maps an overlay texture coordinate ID to the index of the
 * corresponding texture coordinates in the static mesh's UVs array.
 */
using OverlayTextureCoordinateIDMap =
    std::array<int32_t, maximumOverlayTextureCoordinateIDs>;
