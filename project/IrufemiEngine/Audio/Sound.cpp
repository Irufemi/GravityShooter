#include "Audio/Sound.h"
#include <cassert>
#include <propvarutil.h>

// ~Sound() は default のため、ここの定義は不要です

bool Sound::Load(const std::wstring& filePath) {
    HRESULT hr;

    // ソースリーダーの作成
    Microsoft::WRL::ComPtr<IMFSourceReader> pMFSourceReader;
    hr = MFCreateSourceReaderFromURL(filePath.c_str(), NULL, &pMFSourceReader);
    if (FAILED(hr)) {
        return false;
    }

    // メディアタイプをPCMに設定
    Microsoft::WRL::ComPtr<IMFMediaType> pMFMediaType;
    hr = MFCreateMediaType(&pMFMediaType);
    if (FAILED(hr)) {
        return false;
    }

    hr = pMFMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) {
        return false;
    }

    hr = pMFMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) {
        return false;
    }

    hr = pMFSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pMFMediaType.Get());
    if (FAILED(hr)) {
        return false;
    }

    pMFMediaType.Reset();

    hr = pMFSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pMFMediaType);
    if (FAILED(hr)) {
        return false;
    }

    WAVEFORMATEX* rawWaveFormat = nullptr;
    hr = MFCreateWaveFormatExFromMFMediaType(pMFMediaType.Get(), &rawWaveFormat, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    pWaveFormat_.reset(rawWaveFormat);

    pMFMediaType.Reset();

    // データの読み込み
    mediaData_.clear();

    // 再生時間から推定バッファサイズを計算し、事前にメモリを確保してヒープ断片化を防ぐ
    PROPVARIANT var;
    PropVariantInit(&var);
    hr = pMFSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
    if (SUCCEEDED(hr)) {
        if (var.vt == VT_UI8 && pWaveFormat_) {
            // var.uhVal.QuadPart は 100ナノ秒単位 (1秒 = 10,000,000単位)
            LONGLONG durationHns = var.uhVal.QuadPart;
            double durationSec = static_cast<double>(durationHns) / 10000000.0;
            size_t estimatedBytes = static_cast<size_t>(durationSec * pWaveFormat_->nAvgBytesPerSec);
            if (estimatedBytes > 0) {
                mediaData_.reserve(estimatedBytes);
            }
        }
        PropVariantClear(&var);
    }
    while (true) {
        Microsoft::WRL::ComPtr<IMFSample> pMFSample;
        DWORD dwStreamFlags{0};
        hr = pMFSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &dwStreamFlags, nullptr,
                                         &pMFSample);
        if (FAILED(hr)) {
            return false;
        }

        if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        if (!pMFSample) {
            continue;
        }

        Microsoft::WRL::ComPtr<IMFMediaBuffer> pMFMediaBuffer;
        hr = pMFSample->ConvertToContiguousBuffer(&pMFMediaBuffer);
        if (FAILED(hr)) {
            return false;
        }

        BYTE* pBuffer{nullptr};
        DWORD cbCurrentLength{0};
        hr = pMFMediaBuffer->Lock(&pBuffer, nullptr, &cbCurrentLength);
        if (FAILED(hr)) {
            return false;
        }

        size_t currentSize = mediaData_.size();
        mediaData_.resize(currentSize + cbCurrentLength);
        memcpy(mediaData_.data() + currentSize, pBuffer, cbCurrentLength);

        pMFMediaBuffer->Unlock();
    }

    return true;
}