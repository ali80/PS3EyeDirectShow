#pragma once

#include <dshow.h>
#include <cstring>

// Validate the complete header before reading fields or sizing a frame buffer.
// RGB BI_RGB permits biSizeImage == 0; allocation must still use geometry.
inline HRESULT ValidatePS3EyeMediaType(const AM_MEDIA_TYPE *type, LONG *frameBytes)
{
    if (!type || !frameBytes) return E_POINTER;
    *frameBytes = 0;
    if (type->majortype != MEDIATYPE_Video || type->subtype != MEDIASUBTYPE_RGB32 ||
        type->formattype != FORMAT_VideoInfo || !type->pbFormat ||
        type->cbFormat < sizeof(VIDEOINFOHEADER)) return VFW_E_TYPE_NOT_ACCEPTED;

    VIDEOINFOHEADER header;
    std::memcpy(&header, type->pbFormat, sizeof(header));
    const BITMAPINFOHEADER &bitmap = header.bmiHeader;
    if (bitmap.biSize != sizeof(BITMAPINFOHEADER) || bitmap.biPlanes != 1 ||
        bitmap.biBitCount != 32 || bitmap.biCompression != BI_RGB ||
        !((bitmap.biWidth == 640 && bitmap.biHeight == 480) ||
          (bitmap.biWidth == 320 && bitmap.biHeight == 240)) ||
        header.AvgTimePerFrame < 10000000 / 70 ||
        header.AvgTimePerFrame > 10000000 / 2) return VFW_E_TYPE_NOT_ACCEPTED;

    // Dimensions have been restricted above, so this multiplication fits LONG.
    const LONG bytes = bitmap.biWidth * bitmap.biHeight * 4;
    if (bitmap.biSizeImage != 0 && bitmap.biSizeImage != static_cast<DWORD>(bytes))
        return VFW_E_TYPE_NOT_ACCEPTED;
    *frameBytes = bytes;
    return S_OK;
}
