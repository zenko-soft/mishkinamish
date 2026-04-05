#ifndef __MM_WASAPI_INPUT
#define __MM_WASAPI_INPUT

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys.h>
#include "MMGlobals.h"

#define WASAPI_NUM_BUFFERS 4
#define WASAPI_SAMPLE_RATE 16000
#define WASAPI_CHANNEL_COUNT 1
#define WASAPI_BITS_PER_SAMPLE 16

class WasapiInput {
public:
    static bool Init();
    static void Cleanup();
    static int Start(IMMDevice* pDevice);
    static void Stop();
    static void OnSoundData();
    static bool IsActive();
    static void SetDevice(IMMDevice* pDevice);
    static IMMDevice* GetDefaultDevice();
    static IMMDevice* GetDeviceByIndex(int index);
    static int GetDeviceCount();
    static wchar_t* GetDeviceName(IMMDevice* pDevice);
    static bool Reconnect();

    static short buf[WASAPI_NUM_BUFFERS][MM_SOUND_BUFFER_LEN];
    static volatile bool flag_running;

protected:
    static IAudioClient* pAudioClient;
    static IAudioCaptureClient* pCaptureClient;
    static IMMDevice* pDevice;
    static IMMDeviceEnumerator* pEnumerator;
    static HANDLE hCaptureThread;
    static HANDLE hStopEvent;
    static HANDLE hDataReadyEvent;
    static WAVEFORMATEX* pWaveFormat;
    static bool use_mme_fallback;

    static unsigned __stdcall CaptureThread(void* p);
    static HRESULT InitializeAudioClient(IMMDevice* pDevice);
};

#endif
