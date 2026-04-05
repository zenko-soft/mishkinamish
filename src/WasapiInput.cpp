#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "WasapiInput.h"
#include "WorkerThread.h"

#pragma comment(lib, "ole32.lib")

IAudioClient* WasapiInput::pAudioClient = NULL;
IAudioCaptureClient* WasapiInput::pCaptureClient = NULL;
IMMDevice* WasapiInput::pDevice = NULL;
IMMDeviceEnumerator* WasapiInput::pEnumerator = NULL;
HANDLE WasapiInput::hCaptureThread = NULL;
HANDLE WasapiInput::hStopEvent = NULL;
HANDLE WasapiInput::hDataReadyEvent = NULL;
WAVEFORMATEX* WasapiInput::pWaveFormat = NULL;
bool WasapiInput::use_mme_fallback = false;
volatile bool WasapiInput::flag_running = false;

short WasapiInput::buf[WASAPI_NUM_BUFFERS][MM_SOUND_BUFFER_LEN] = {0};

bool WasapiInput::Init() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    if (FAILED(hr)) {
        return false;
    }

    hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    hDataReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    return true;
}

void WasapiInput::Cleanup() {
    Stop();

    if (hStopEvent) {
        CloseHandle(hStopEvent);
        hStopEvent = NULL;
    }

    if (hDataReadyEvent) {
        CloseHandle(hDataReadyEvent);
        hDataReadyEvent = NULL;
    }

    if (pWaveFormat) {
        CoTaskMemFree(pWaveFormat);
        pWaveFormat = NULL;
    }

    if (pEnumerator) {
        pEnumerator->Release();
        pEnumerator = NULL;
    }

    CoUninitialize();
}

IMMDevice* WasapiInput::GetDefaultDevice() {
    if (!pEnumerator) return NULL;

    IMMDevice* pDefaultDevice = NULL;
    HRESULT hr = pEnumerator->GetDefaultAudioEndpoint(
        eCapture,
        eCommunications,
        &pDefaultDevice);

    if (FAILED(hr)) {
        hr = pEnumerator->GetDefaultAudioEndpoint(
            eCapture,
            eConsole,
            &pDefaultDevice);
    }

    return pDefaultDevice;
}

IMMDevice* WasapiInput::GetDeviceByIndex(int index) {
    if (!pEnumerator) return NULL;

    IMMDeviceCollection* pCollection = NULL;
    HRESULT hr = pEnumerator->EnumAudioEndpoints(
        eCapture,
        DEVICE_STATE_ACTIVE,
        &pCollection);

    if (FAILED(hr)) return NULL;

    IMMDevice* pDeviceResult = NULL;
    hr = pCollection->Item(index, &pDeviceResult);

    pCollection->Release();

    if (FAILED(hr)) return NULL;
    return pDeviceResult;
}

int WasapiInput::GetDeviceCount() {
    if (!pEnumerator) return 0;

    IMMDeviceCollection* pCollection = NULL;
    HRESULT hr = pEnumerator->EnumAudioEndpoints(
        eCapture,
        DEVICE_STATE_ACTIVE,
        &pCollection);

    if (FAILED(hr)) return 0;

    UINT count = 0;
    hr = pCollection->GetCount(&count);

    pCollection->Release();

    return SUCCEEDED(hr) ? count : 0;
}

wchar_t* WasapiInput::GetDeviceName(IMMDevice* pDevice) {
    if (!pDevice) return NULL;

    static wchar_t deviceName[256] = L"";

    IPropertyStore* pPropertyStore = NULL;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);

    if (SUCCEEDED(hr) && pPropertyStore) {
        PROPVARIANT varName;
        PropVariantInit(&varName);

        hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &varName);

        if (SUCCEEDED(hr) && varName.pwszVal) {
            wcscpy_s(deviceName, varName.pwszVal);
            PropVariantClear(&varName);
        } else {
            wcscpy_s(deviceName, L"Unknown Device");
        }

        pPropertyStore->Release();
    }

    return deviceName;
}

HRESULT WasapiInput::InitializeAudioClient(IMMDevice* pDevice) {
    if (!pDevice) return E_FAIL;

    HRESULT hr = pDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        NULL,
        (void**)&pAudioClient);

    if (FAILED(hr)) {
        return hr;
    }

    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = WASAPI_CHANNEL_COUNT;
    waveFormat.nSamplesPerSec = WASAPI_SAMPLE_RATE;
    waveFormat.wBitsPerSample = WASAPI_BITS_PER_SAMPLE;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    REFERENCE_TIME hnsDefaultDevicePeriod = 0;
    REFERENCE_TIME hnsMinimumDevicePeriod = 0;

    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsMinimumDevicePeriod);
    if (FAILED(hr)) {
        return hr;
    }

    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hnsDefaultDevicePeriod,
        0,
        &waveFormat,
        NULL);

    if (FAILED(hr)) {
        pAudioClient->Release();
        pAudioClient = NULL;
        return hr;
    }

    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pCaptureClient);

    if (FAILED(hr)) {
        pAudioClient->Release();
        pAudioClient = NULL;
        return hr;
    }

    pAudioClient->SetEventHandle(hDataReadyEvent);

    return S_OK;
}

int WasapiInput::Start(IMMDevice* pNewDevice) {
    if (flag_running) {
        Stop();
    }

    pDevice = pNewDevice;
    if (!pDevice) {
        pDevice = GetDefaultDevice();
    }

    if (!pDevice) {
        use_mme_fallback = true;
        return 1;
    }

    HRESULT hr = InitializeAudioClient(pDevice);
    if (FAILED(hr)) {
        use_mme_fallback = true;
        pDevice->Release();
        pDevice = NULL;
        return 1;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        use_mme_fallback = true;
        if (pCaptureClient) {
            pCaptureClient->Release();
            pCaptureClient = NULL;
        }
        if (pAudioClient) {
            pAudioClient->Release();
            pAudioClient = NULL;
        }
        pDevice->Release();
        pDevice = NULL;
        return 1;
    }

    ResetEvent(hStopEvent);
    hCaptureThread = (HANDLE)_beginthreadex(NULL, 0, CaptureThread, NULL, 0, NULL);
    flag_running = true;

    return 0;
}

void WasapiInput::Stop() {
    if (!flag_running) return;

    flag_running = false;

    if (hStopEvent) {
        SetEvent(hStopEvent);
    }

    if (hCaptureThread) {
        WaitForSingleObject((HANDLE)hCaptureThread, 1000);
        CloseHandle((HANDLE)hCaptureThread);
        hCaptureThread = NULL;
    }

    if (pAudioClient) {
        pAudioClient->Stop();
    }

    if (pCaptureClient) {
        pCaptureClient->Release();
        pCaptureClient = NULL;
    }

    if (pAudioClient) {
        pAudioClient->Release();
        pAudioClient = NULL;
    }

    if (pDevice) {
        pDevice->Release();
        pDevice = NULL;
    }
}

void WasapiInput::SetDevice(IMMDevice* pNewDevice) {
    if (flag_running && pNewDevice != pDevice) {
        Stop();
        Start(pNewDevice);
    }
}

bool WasapiInput::IsActive() {
    return flag_running && !use_mme_fallback;
}

bool WasapiInput::Reconnect() {
    if (!flag_running) return false;

    Stop();

    IMMDevice* pNewDevice = GetDefaultDevice();
    if (!pNewDevice) return false;

    int result = Start(pNewDevice);
    return result == 0;
}

unsigned __stdcall WasapiInput::CaptureThread(void* p) {
    HANDLE handles[2] = { hStopEvent, hDataReadyEvent };

    while (!flag_running || WaitForMultipleObjects(2, handles, FALSE, 100) == WAIT_TIMEOUT) {
        if (!flag_running) break;

        DWORD result = WaitForMultipleObjects(2, handles, FALSE, 100);
        
        if (result == WAIT_OBJECT_0) {
            break;
        }
        else if (result == WAIT_OBJECT_0 + 1) {
            OnSoundData();
        }
    }

    return 0;
}

void WasapiInput::OnSoundData() {
    if (!pCaptureClient) return;

    UINT32 packetLength = 0;
    HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);

    if (FAILED(hr) || packetLength == 0) return;

    while (packetLength > 0) {
        BYTE* pData = NULL;
        UINT32 framesToRead = 0;
        DWORD flags = 0;

        hr = pCaptureClient->GetBuffer(&pData, &framesToRead, &flags, NULL, NULL);

        if (FAILED(hr) || framesToRead == 0) break;

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            memset(buf[0], 0, MM_SOUND_BUFFER_LEN * sizeof(short));
        } else {
            int samplesToCopy = min(framesToRead, (UINT32)MM_SOUND_BUFFER_LEN);
            memcpy(buf[0], pData, samplesToCopy * sizeof(short));

            if (samplesToCopy < MM_SOUND_BUFFER_LEN) {
                memset(&buf[0][samplesToCopy], 0, (MM_SOUND_BUFFER_LEN - samplesToCopy) * sizeof(short));
            }
        }

        WorkerThread::PushData(buf[0]);

        hr = pCaptureClient->ReleaseBuffer(framesToRead);
        if (FAILED(hr)) break;

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;
    }
}
