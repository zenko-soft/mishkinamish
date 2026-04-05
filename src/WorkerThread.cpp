#include <Windows.h>

#include <math.h>
#include <process.h>
#include "ClickSound.h"
#include "CopyShmopy.h"
#include "KChFstate.h"
#include "WAVDump.h"
#include "WAVLoader.h"
#include "WorkerThread.h"

#define MM_WORKER_BUFFERS_IN_CIRCLE 4

extern bool f_reading_file;

extern volatile bool flag_wav_dump;
extern volatile bool flag_training_mode;
extern volatile long training_frame_counter;
int volatile WorkerThread::training_silence_indicator;

static volatile bool flag_ShutDownWorkerThread =
    false;
volatile HANDLE hInput2WorkerDataReady = NULL;

static volatile LONG num_free_buf =
    MM_WORKER_BUFFERS_IN_CIRCLE;

int WorkerThread::last_read_buffer = 0;
int WorkerThread::last_write_buffer = 0;

int volatile WorkerThread::indicator_value;

uintptr_t WorkerThread::worker_thread_handle = NULL;

short WorkerThread::input_buf[MM_SOUND_BUFFER_LEN];
volatile LONG WorkerThread::flag_input_buffer_ready = 1;

short WorkerThread::output_buf[MM_NUM_WORKER_OUTPUT_BUFFERS]
                              [MM_SOUND_BUFFER_LEN];

CRITICAL_SECTION WorkerThread::cs_input;

int volatile g_click_sound = 0;
extern volatile bool
    flag_keep_silence;

void WorkerThread_Init() {
    InitializeCriticalSection(&WorkerThread::cs_input);
}

void WorkerThread_Destroy() {
    DeleteCriticalSection(&WorkerThread::cs_input);
}

unsigned __stdcall FuncWorkerThread(void *p) {
  while (!flag_ShutDownWorkerThread) {
    DWORD result = WaitForSingleObject(hInput2WorkerDataReady, 100);
    if (result == WAIT_OBJECT_0) {
      WorkerThread::Work();
    }
  }
  return 0;
}

void WorkerThread::Start(HWND hdwnd) {
  WorkerThread_Init();
  if (!hInput2WorkerDataReady) {
    hInput2WorkerDataReady = CreateEvent(0, FALSE, FALSE, 0);
  }

  if (!worker_thread_handle) {
    worker_thread_handle =
        _beginthreadex(NULL, 0, FuncWorkerThread, 0, 0, NULL);
  }
}

void WorkerThread::Halt(HWND hdwnd) {
  WorkerThread_Destroy();
  flag_ShutDownWorkerThread = true;
  if (hInput2WorkerDataReady) {
    SetEvent(hInput2WorkerDataReady);
  }
  if (worker_thread_handle) {
    WaitForSingleObject((HANDLE)worker_thread_handle, 500);
    CloseHandle((HANDLE)worker_thread_handle);
    worker_thread_handle = NULL;
  }
  if (hInput2WorkerDataReady) {
    CloseHandle(hInput2WorkerDataReady);
    hInput2WorkerDataReady = NULL;
  }
}

void WorkerThread::PushData(short *_buf) {
  LONG expected = 1;
  if (InterlockedCompareExchange(&flag_input_buffer_ready, 0, expected) == expected) {
    EnterCriticalSection(&cs_input);
    memcpy(input_buf, _buf, MM_SOUND_BUFFER_LEN * sizeof(short));
    LeaveCriticalSection(&cs_input);
    MemoryBarrier();
    if (hInput2WorkerDataReady) SetEvent(hInput2WorkerDataReady);
  }
}

int WorkerThread::PullData(short *_buf) {
  LONG current_free = InterlockedCompareExchange(&num_free_buf, 0, 0);
  if (MM_WORKER_BUFFERS_IN_CIRCLE > current_free) {
    last_read_buffer++;
    if (last_read_buffer >= MM_NUM_WORKER_OUTPUT_BUFFERS) last_read_buffer = 0;

    memcpy(_buf,
           output_buf[last_read_buffer],
           MM_SOUND_BUFFER_LEN * sizeof(short));

    InterlockedIncrement(&num_free_buf);

    if (f_reading_file) SetEvent(hInput2WorkerDataReady);

    return 0;
  }
  return 1;
}

void WorkerThread::Work() {
  LONG ready = InterlockedCompareExchange(&flag_input_buffer_ready, 1, 0);
  if (ready != 0) {
    return;
  }

  bool skipped = true;
  LONG current_free = InterlockedCompareExchange(&num_free_buf, 0, 0);
  if (current_free > 0) {
    skipped = false;
    last_write_buffer++;
    if (last_write_buffer >= MM_NUM_WORKER_OUTPUT_BUFFERS)
      last_write_buffer = 0;
  }

  EnterCriticalSection(&cs_input);
  short local_buf[MM_SOUND_BUFFER_LEN];
  memcpy(local_buf, input_buf, MM_SOUND_BUFFER_LEN * sizeof(short));
  LeaveCriticalSection(&cs_input);

  if (f_reading_file)
    MMWAVLoader::FillBuffer(local_buf);

  double average_value = 0.0;
  for (int i = 0; i < MM_SOUND_BUFFER_LEN; i++) {
    average_value += local_buf[i] * local_buf[i];
  }
  average_value /= MM_SOUND_BUFFER_LEN;

  double l = log((double)average_value);

  indicator_value = (int)(l / 1.9) - 3;
  if (indicator_value < 0) indicator_value = 0;
  if (indicator_value > 7) indicator_value = 7;

  if (flag_training_mode) {
    if (0 == training_frame_counter) training_silence_indicator = 0;

    if ((training_frame_counter >= 10) && (training_frame_counter < 40)) {
      training_silence_indicator += indicator_value;
    }
    if (40 == training_frame_counter)
      training_silence_indicator /= 30;

    training_frame_counter++;
  }

  KChFstate::NewFrame(indicator_value);

  if (flag_wav_dump)
    flag_wav_dump = MMWAVDump::DumpBuffer(local_buf, sizeof(local_buf));

  CopyShmopy::Process(output_buf[last_write_buffer], local_buf);

  if (flag_keep_silence)
    ZeroMemory(output_buf[last_write_buffer],
               sizeof(output_buf[last_write_buffer]));

  if (g_click_sound) {
    ClickSound::AddSound(output_buf[last_write_buffer], g_click_sound);
    g_click_sound = 0;
  }

  if (!skipped) {
    InterlockedDecrement(&num_free_buf);
  }

  MemoryBarrier();
  InterlockedExchange(&flag_input_buffer_ready, 1);

  if (f_reading_file && (num_free_buf > 0)) SetEvent(hInput2WorkerDataReady);
}
