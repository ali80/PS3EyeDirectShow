// Regression tests use real DirectShow allocators and the production pin.
// No camera, registration, elevation, network, or capture is required.
#include <streams.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include "ps3eye.h"
#include "PS3EyeSourceFilter.h"

static int checks = 0;
static void expect(bool condition, const char *message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

class TestSource : public CSource {
public:
    TestSource() : CSource(NAME("Test source"), NULL, CLSID_NULL) {}
};

static void testSamples(PS3EyePushPin *pin, LONG size, HRESULT expectedResult)
{
    HRESULT hr = S_OK;
    CMemAllocator *allocator = new CMemAllocator(NAME("Test allocator"), NULL, &hr);
    expect(SUCCEEDED(hr), "allocator construction");
    allocator->AddRef();
    ALLOCATOR_PROPERTIES wanted = {1, size, 1, 0}, actual = {};
    expect(SUCCEEDED(allocator->SetProperties(&wanted, &actual)), "allocator properties");
    expect(SUCCEEDED(allocator->Commit()), "allocator commit");
    IMediaSample *sample = NULL;
    expect(SUCCEEDED(allocator->GetBuffer(&sample, NULL, NULL, 0)), "allocate sample");
    BYTE *data = NULL;
    expect(SUCCEEDED(sample->GetPointer(&data)), "sample pointer");
    std::memset(data, 0xa5, sample->GetSize());
    expect(pin->FillBuffer(sample) == expectedResult, "sample capacity contract");
    const LONG frameBytes = 640 * 480 * 4;
    if (FAILED(expectedResult)) {
        for (LONG i = 0; i < sample->GetSize(); ++i)
            if (data[i] != 0xa5) throw std::runtime_error("rejected sample was modified");
    } else {
        expect(sample->GetActualDataLength() == frameBytes, "actual frame length");
        for (LONG i = frameBytes; i < sample->GetSize(); ++i)
            if (data[i] != 0xa5) throw std::runtime_error("write beyond frame boundary");
    }
    sample->Release();
    allocator->Decommit();
    allocator->Release();
}

int main()
{
    HRESULT init = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(init)) return 2;
    try {
        TestSource *source = new TestSource();
        source->AddRef();
        HRESULT hr = S_OK;
        PS3EyePushPin *pin = new PS3EyePushPin(&hr, source, ps3eye::PS3EYECam::PS3EYERef());
        expect(SUCCEEDED(hr), "pin construction");
        CMediaType valid;
        expect(pin->GetMediaType(0, &valid) == S_OK, "default format");
        expect(pin->CheckMediaType(&valid) == S_OK, "valid format accepted");
        expect(pin->CheckMediaType(NULL) == E_POINTER, "null format rejected");
        expect(pin->SetFormat(NULL) == E_POINTER, "null SetFormat rejected");

        // Preserve the full backing allocation while lying about its length.
        // The old implementation accepted these truncated format blocks.
        for (ULONG length = 0; length < sizeof(VIDEOINFOHEADER); ++length) {
            CMediaType truncated(valid);
            truncated.cbFormat = length;
            expect(FAILED(pin->CheckMediaType(&truncated)), "truncated VIDEOINFOHEADER rejected");
        }
        AM_MEDIA_TYPE missing = valid;
        missing.pbFormat = NULL;
        expect(FAILED(pin->SetFormat(&missing)), "null format payload rejected before copying");
        CMediaType bad(valid);
        ((VIDEOINFOHEADER*)bad.Format())->bmiHeader.biSizeImage = 4;
        expect(FAILED(pin->CheckMediaType(&bad)), "undersized biSizeImage rejected");
        ((VIDEOINFOHEADER*)bad.Format())->bmiHeader.biSizeImage = 0xffffffffu;
        expect(FAILED(pin->CheckMediaType(&bad)), "oversized biSizeImage rejected");
        bad = valid;
        ((VIDEOINFOHEADER*)bad.Format())->bmiHeader.biSize = 1;
        expect(FAILED(pin->CheckMediaType(&bad)), "invalid bitmap header size rejected");
        bad = valid;
        ((VIDEOINFOHEADER*)bad.Format())->bmiHeader.biWidth = 0x7fffffff;
        expect(FAILED(pin->CheckMediaType(&bad)), "overflowing width rejected");
        bad = valid;
        ((VIDEOINFOHEADER*)bad.Format())->bmiHeader.biHeight = -480;
        expect(FAILED(pin->CheckMediaType(&bad)), "unsupported orientation rejected");
        bad = valid;
        ((VIDEOINFOHEADER*)bad.Format())->AvgTimePerFrame = 0;
        expect(FAILED(pin->CheckMediaType(&bad)), "zero frame interval rejected");
        bad = valid;
        ((VIDEOINFOHEADER*)bad.Format())->bmiHeader.biSizeImage = 0;
        expect(pin->SetFormat(&bad) == S_OK, "BI_RGB zero image size accepted");
        AM_MEDIA_TYPE *canonical = NULL;
        expect(pin->GetFormat(&canonical) == S_OK, "get canonical format");
        expect(((VIDEOINFOHEADER*)canonical->pbFormat)->bmiHeader.biSizeImage == 640 * 480 * 4,
               "canonical format uses geometry size");
        expect(canonical->lSampleSize == 640 * 480 * 4, "canonical sample size");
        DeleteMediaType(canonical);
        expect(pin->SetMediaType(&bad) == S_OK, "connect BI_RGB zero image size");
        expect(pin->FillBuffer(NULL) == E_POINTER, "null sample rejected");
        testSamples(pin, 640 * 480 * 4 - 4, VFW_E_BUFFER_OVERFLOW);
        testSamples(pin, 640 * 480 * 4 + 64, S_OK);

        DWORD returned = 0, supported = 0;
        expect(pin->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, NULL, 0, &returned) == S_OK,
               "property size query");
        expect(returned == sizeof(GUID), "property returned byte count");
        expect(pin->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, NULL, 0, NULL) == E_POINTER,
               "null property outputs rejected");
        GUID category = {};
        expect(FAILED(pin->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &category, 1, &returned)),
               "short property buffer rejected");
        expect(pin->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &category, sizeof(category), &returned) == S_OK,
               "capture property");
        expect(category == PIN_CATEGORY_CAPTURE, "capture category value");
        expect(pin->QuerySupported(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL) == E_POINTER,
               "null support output rejected");
        expect(pin->QuerySupported(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, &supported) == S_OK &&
               supported == KSPROPERTY_SUPPORT_GET, "property support flags");
        int count = 0, size = 0;
        expect(pin->GetNumberOfCapabilities(&count, &size) == S_OK && count == 6,
               "six advertised formats");
        for (int i = 0; i < count; ++i) {
            VIDEO_STREAM_CONFIG_CAPS caps = {};
            AM_MEDIA_TYPE *format = NULL;
            expect(pin->GetStreamCaps(i, &format, reinterpret_cast<BYTE*>(&caps)) == S_OK, "stream capability");
            CMediaType checked(*format);
            expect(pin->CheckMediaType(&checked) == S_OK, "advertised format accepted");
            expect(caps.guid == FORMAT_VideoInfo && caps.MaxBitsPerSecond > caps.MinBitsPerSecond,
                   "valid capability metadata");
            DeleteMediaType(format);
        }
        VIDEO_STREAM_CONFIG_CAPS caps = {};
        AM_MEDIA_TYPE *invalid = reinterpret_cast<AM_MEDIA_TYPE*>(1);
        expect(FAILED(pin->GetStreamCaps(6, &invalid, reinterpret_cast<BYTE*>(&caps))) && invalid == NULL,
               "invalid capability clears output");
        expect(FAILED(pin->OnThreadCreate()), "missing camera fails instead of streaming blank frames");
        source->Release(); // Owns and destroys its pin.
        std::cout << checks << " capture contract checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        CoUninitialize();
        return 1;
    }
    CoUninitialize();
    return 0;
}
