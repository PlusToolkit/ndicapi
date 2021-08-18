/*=======================================================================

Copyright (c) 2000-2005 Atamai, Inc.

Use, modification and redistribution of the software, in source or
binary forms, are permitted provided that the following terms and
conditions are met:

1) Redistribution of the source code, in verbatim or modified
   form, must retain the above copyright notice, this license,
   the following disclaimer, and any notices that refer to this
   license and/or the following disclaimer.

2) Redistribution in binary form must include the above copyright
   notice, a copy of this license and the following disclaimer
   in the documentation or with other materials provided with the
   distribution.

3) Modified copies of the source code must be clearly marked as such,
   and must not be misrepresented as verbatim copies of the source code.

THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE THE SOFTWARE "AS IS"
WITHOUT EXPRESSED OR IMPLIED WARRANTY INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  IN NO EVENT SHALL ANY COPYRIGHT HOLDER OR OTHER PARTY WHO MAY
MODIFY AND/OR REDISTRIBUTE THE SOFTWARE UNDER THE TERMS OF THIS LICENSE
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, LOSS OF DATA OR DATA BECOMING INACCURATE
OR LOSS OF PROFIT OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF
THE USE OR INABILITY TO USE THE SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGES.

=======================================================================*/

/*! \file ndicapi_thread.h
  This file contains the platform-dependent portions of the
  NDICAPI C API for doing thread handling.
*/

#ifndef NDICAPI_THREAD_H
#define NDICAPI_THREAD_H

#include "ndicapiExport.h"

/*=====================================================================*/
/*! \defgroup NDIThread NDI Thread Methods
  These are low-level methods that provide a platform-independent
  multithreading.
*/


#ifdef _WIN32

#include <windows.h>
#include <winbase.h>
#include <sys/timeb.h>

typedef HANDLE NDIThread;
typedef HANDLE NDIMutex;
typedef HANDLE NDIEvent;

#elif defined(unix) || defined(__unix__) || defined(__APPLE__)

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

typedef struct
{
  int signalled;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
} pl_cond_and_mutex_t;
typedef pthread_t NDIThread;
typedef pthread_mutex_t* NDIMutex;
typedef pl_cond_and_mutex_t* NDIEvent;

#endif

#ifdef __cplusplus
extern "C" {
#endif

ndicapiExport NDIMutex ndiMutexCreate();
ndicapiExport void ndiMutexDestroy(NDIMutex mutex);
ndicapiExport void ndiMutexLock(NDIMutex mutex);
ndicapiExport void ndiMutexUnlock(NDIMutex mutex);

ndicapiExport NDIEvent ndiEventCreate();
ndicapiExport void ndiEventDestroy(NDIEvent event);
ndicapiExport void ndiEventSignal(NDIEvent event);
ndicapiExport int ndiEventWait(NDIEvent event, int milliseconds);

ndicapiExport NDIThread ndiThreadSplit(void* thread_func(void* userdata), void* userdata);
ndicapiExport void ndiThreadJoin(NDIThread Thread);

#ifdef __cplusplus
}
#endif

#endif