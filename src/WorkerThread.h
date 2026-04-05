#ifndef __MM_WORKER_THREAD
#define __MM_WORKER_THREAD

#include <Windows.h>
#include "MMGlobals.h"

#define MM_NUM_WORKER_OUTPUT_BUFFERS 4

class WorkerThread {
 public:
  static void Start(
      HWND hdwnd = 0);
  static void Halt(HWND hdwnd = 0);
  static void Work();
  static void PushData(short *_buf);
  static int PullData(short *_buf);

  static volatile int indicator_value;
  static volatile int training_silence_indicator;
  static volatile LONG flag_input_buffer_ready;
  static CRITICAL_SECTION cs_input;

 protected:
  static uintptr_t worker_thread_handle;
  static short input_buf[MM_SOUND_BUFFER_LEN];
  static short output_buf[MM_NUM_WORKER_OUTPUT_BUFFERS][MM_SOUND_BUFFER_LEN];
  static int last_read_buffer;
  static int last_write_buffer;
};

void WorkerThread_Init();
void WorkerThread_Destroy();

#endif
