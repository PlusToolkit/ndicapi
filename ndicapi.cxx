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

#if defined(_MSC_VER)
  #pragma warning ( disable : 4996 )
#endif

#include "ndicapi.h"
#include "ndicapi_socket.h"
#include "ndicapi_thread.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if defined(__APPLE__)
  #include <dirent.h>
#endif

#ifdef __cplusplus
  #include <assert.h>
  #include <sstream>
#endif

//----------------------------------------------------------------------------
// Structure for holding ndicapi data.
//
// This structure is defined in this C file because its members are not
// directly accessed anywhere except for this C file.


// Max tools is 12 active plus 9 passive, so 24 is a safe number
// (note that we are only counting the number of handles that can
// be simultaneously occupied)
#define NDI_MAX_HANDLES 24

struct ndicapi
{
  // low-level communication information
  NDIFileHandle SerialDevice;             // file handle for ndicapi
  char* SerialDeviceName;                 // device name for ndicapi

  NDISocketHandle Socket;                 // socket handle
  char* Hostname;                         // socket hostname
  int Port;                               // socket port
  int SocketErrorCode;                    // error code (zero if no error)

  char* Command;                          // text sent to the ndicapi
  char* Reply;                            // reply from the ndicapi

  // this is set to true during tracking mode
  bool IsTracking;

  // low-level threading information
  bool IsThreadedMode;                    // flag for threading mode
  NDIThread Thread;                       // the thread handle
  NDIMutex ThreadMutex;                   // for blocking the thread
  NDIMutex ThreadBufferMutex;             // lock the reply buffer
  NDIEvent ThreadBufferEvent;             // for when buffer is updated
  char* ThreadCommand;                    // last command sent from thread
  char* ThreadReply;                      // reply from the ndicapi
  char* ThreadBuffer;                     // buffer for previous reply
  bool IsThreadedCommandBinary;           // cache whether we're sending BX (true) or TX/GX (false)
  int ThreadErrorCode;                    // error code to go with buffer

  // command reply -- this is the return value from plCommand()
  char* ReplyNoCRC;                     // reply without CRC and <CR>

  // error handling information
  int ErrorCode;                          // error code (zero if no error)
  // Error callback
  void (*ErrorCallback)(int code, char* description, void* data);
  void* ErrorCallbackData;                // user data for callback

  // GX command reply data
  char GxTransforms[3][52];               // 3 active tool transforms
  char GxStatus[8];                       // tool and system status
  char GxInformation[3][12];              // extra transform information
  char GxSingleStray[3][24];              // one stray marker per tool
  char GxFrame[3][8];                     // frame number for each tool

  char GxPassiveTransforms[9][52];        // 9 passive tool transforms
  char GxPassiveStatus[24];               // tool and system status
  char GxPassiveInformation[9][12];       // extra transform information
  char GxPassiveFrame[9][8];              // frame number for each tool

  char GxPassiveStray[424];               // all passive stray markers

  // PSTAT command reply data
  char PstatBasic[3][32];                 // basic pstat info
  char PstatTesting[3][8];                // testing results
  char PstatPartNumber[3][20];            // part number
  char PstatAccessories[3][2];            // accessory information
  char PstatMarkerType[3][2];             // marker information

  char PstatPassiveBasic[9][32];          // basic passive pstat info
  char PstatPassiveTesting[9][8];         // meaningless info
  char PstatPassivePartNumber[9][20];     // virtual srom part number
  char PstatPassiveAccessories[9][2];     // virtual srom accessories
  char PstatPassiveMarkerType[9][2];      // meaningless for passive

  // SSTAT command reply data
  char SstatControl[2];                   // control processor status
  char SstatSensor[2];                    // sensor processors status
  char SstatTiu[2];                       // tiu processor status

  // IRCHK command reply data
  int IrchkDetected;                      // irchk detected infrared
  char IrchkSources[128];                 // coordinates of sources

  // PHRQ command reply data
  char PhrqReply[2];

  // PHSR command reply data
  char PhsrReply[1284];

  // PHINF command reply data
  int PhinfUnoccupied;
  char PhinfBasic[34];
  char PhinfTesting[8];
  char PhinfPartNumber[20];
  char PhinfAccessories[2];
  char PhinfMarkerType[2];
  char PhinfPortLocation[14];
  char PhinfGpioStatus[2];

  // TX command reply data
  int TxHandleCount;
  unsigned char TxHandles[NDI_MAX_HANDLES];
  char TxTransforms[NDI_MAX_HANDLES][52];
  char TxStatus[NDI_MAX_HANDLES][8];
  char TxFrame[NDI_MAX_HANDLES][8];
  char TxInformation[NDI_MAX_HANDLES][12];
  char TxSingleStray[NDI_MAX_HANDLES][24];
  char TxSystemStatus[4];

  int TxPassiveStrayCount;
  char TxPassiveStrayOov[14];
  char TxPassiveStray[1052];

  // BX command reply data
  unsigned char BxHandleCount;
  char BxHandles[NDI_MAX_HANDLES];
  char BxHandlesStatus[NDI_MAX_HANDLES];
  unsigned int BxFrameNumber[NDI_MAX_HANDLES];
  float BxTransforms[NDI_MAX_HANDLES][8];
  int BxPortStatus[NDI_MAX_HANDLES];
  char BxToolMarkerInformation[NDI_MAX_HANDLES][11];

  char BxActiveSingleStrayMarkerStatus[NDI_MAX_HANDLES];
  float BxActiveSingleStrayMarkerPosition[NDI_MAX_HANDLES][3];

  char Bx3DMarkerCount[NDI_MAX_HANDLES];
  char Bx3DMarkerOutOfVolume[NDI_MAX_HANDLES][3]; // 3 bytes holds 1 bit entries for up to 24 markers (but 20 is max)
  float Bx3DMarkerPosition[NDI_MAX_HANDLES][20][3]; // a tool can have up to 20 markers

  int BxPassiveStrayCount;
  char BxPassiveStrayOutOfVolume[30]; // 30 bytes holds 1 bit entries for up to 240 markers
  float BxPassiveStrayPosition[240][3]; // hold up to 240 stray markers

  int BxSystemStatus;
};

//----------------------------------------------------------------------------
// Prototype for the error helper function, the definition is at the
// end of this file.  A call to this function will both set ndicapi
// error indicator, and will also call the error callback function if
// there is one.  The return value is equal to errnum.
namespace
{
  int ndiSetError(ndicapi* pol, int errnum)
  {
    pol->ErrorCode = errnum;

    // call the user-supplied callback function
    if (pol->ErrorCallback)
    {
      pol->ErrorCallback(errnum, ndiErrorString(errnum), pol->ErrorCallbackData);
    }

    return errnum;
  }
}

//----------------------------------------------------------------------------
ndicapiExport void ndiSetErrorCallback(ndicapi* pol, NDIErrorCallback callback, void* userdata)
{
  pol->ErrorCallback = callback;
  pol->ErrorCallbackData = userdata;
}

//----------------------------------------------------------------------------
ndicapiExport void ndiLogState(ndicapi* pol, char outInformation[USHRT_MAX])
{
#ifdef __cplusplus
  std::stringstream ss;
  for (int i = 0; i < 3; ++i)
  {
    ss << "GxTransforms[" << i << "][52]: " << pol->GxTransforms[i] << std::endl;
  }
  ss << "GxStatus[8]: " << pol->GxStatus;
  for (int i = 0; i < 3; ++i)
  {
    ss << "GxInformation[" << i << "][12]: " << pol->GxInformation[i] << std::endl;
  }
  for (int i = 0; i < 3; ++i)
  {
    ss << "GxSingleStray[" << i << "][24]: " << pol->GxSingleStray[i] << std::endl;
  }
  for (int i = 0; i < 3; ++i)
  {
    ss << "GxFrame[" << i << "][8]: " << pol->GxFrame[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "GxPassiveTransforms[" << i << "][52]: " << pol->GxPassiveTransforms[i] << std::endl;
  }
  ss << "GxPassiveStatus[24]: " << pol->GxPassiveStatus;
  for (int i = 0; i < 9; ++i)
  {
    ss << "GxPassiveInformation[" << i << "][12]: " << pol->GxPassiveInformation[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "GxPassiveFrame[" << i << "][8]: " << pol->GxPassiveFrame[i] << std::endl;
  }
  ss << "GxPassiveStray[424]: " << pol->GxPassiveStray;
  for (int i = 0; i < 3; ++i)
  {
    ss << "PstatBasic[" << i << "][32]: " << pol->PstatBasic[i] << std::endl;
  }
  for (int i = 0; i < 3; ++i)
  {
    ss << "PstatTesting[" << i << "][8]: " << pol->PstatTesting[i] << std::endl;
  }
  for (int i = 0; i < 3; ++i)
  {
    ss << "PstatPartNumber[" << i << "][20]: " << pol->PstatPartNumber[i] << std::endl;
  }
  for (int i = 0; i < 3; ++i)
  {
    ss << "PstatAccessories[" << i << "][2]: " << pol->PstatAccessories[i] << std::endl;
  }
  for (int i = 0; i < 3; ++i)
  {
    ss << "PstatMarkerType[" << i << "][2]: " << pol->PstatMarkerType[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "PstatPassiveBasic[" << i << "][32]: " << pol->PstatPassiveBasic[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "PstatPassiveTesting[" << i << "][8]: " << pol->PstatPassiveTesting[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "PstatPassivePartNumber[" << i << "][20]: " << pol->PstatPassivePartNumber[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "PstatPassiveAccessories[" << i << "][2]: " << pol->PstatPassiveAccessories[i] << std::endl;
  }
  for (int i = 0; i < 9; ++i)
  {
    ss << "PstatPassiveMarkerType[" << i << "][2]: " << pol->PstatPassiveMarkerType[i] << std::endl;
  }
  ss << "SstatControl[2]: " << pol->SstatControl << std::endl;
  ss << "SstatSensor[2]: " << pol->SstatSensor << std::endl;
  ss << "SstatTiu[2]: " << pol->SstatTiu << std::endl;
  ss << "IrchkDetected: " << pol->IrchkDetected << std::endl;
  ss << "IrchkSources[128]: " << pol->IrchkSources << std::endl;
  ss << "PhrqReply[2]: " << pol->PhrqReply << std::endl;
  ss << "PhsrReply[1284]: " << pol->PhsrReply << std::endl;
  ss << "PhinfUnoccupied: " << pol->PhinfUnoccupied << std::endl;
  ss << "PhinfBasic[34]: " << pol->PhinfBasic << std::endl;
  ss << "PhinfTesting[8]: " << pol->PhinfTesting << std::endl;
  ss << "PhinfPartNumber[20]: " << pol->PhinfPartNumber << std::endl;
  ss << "PhinfAccessories[2]: " << pol->PhinfAccessories << std::endl;
  ss << "PhinfMarkerType[2]: " << pol->PhinfMarkerType << std::endl;
  ss << "PhinfPortLocation[14]: " << pol->PhinfPortLocation << std::endl;
  ss << "PhinfGpioStatus[2]: " << pol->PhinfGpioStatus << std::endl;
  ss << "TxHandleCount: " << pol->TxHandleCount << std::endl;
  ss << "TxHandles[NDI_MAX_HANDLES]: " << pol->TxHandles << std::endl;
  for (int i = 0; i < pol->TxHandleCount; ++i)
  {
    ss << "TxTransforms[" << i << "][52]: " << pol->TxTransforms[i] << std::endl;
  }
  for (int i = 0; i < pol->TxHandleCount; ++i)
  {
    ss << "TxStatus[" << i << "][8]: " << pol->TxStatus[i] << std::endl;
  }
  for (int i = 0; i < pol->TxHandleCount; ++i)
  {
    ss << "TxFrame[" << i << "][8]: " << pol->TxFrame[i] << std::endl;
  }
  for (int i = 0; i < pol->TxHandleCount; ++i)
  {
    ss << "TxInformation[" << i << "][12]: " << pol->TxInformation[i] << std::endl;
  }
  for (int i = 0; i < pol->TxHandleCount; ++i)
  {
    ss << "TxSingleStray[" << i << "][24]: " << pol->TxSingleStray[i] << std::endl;
  }
  ss << "TxSystemStatus[4]: " << pol->TxSystemStatus << std::endl;
  ss << "TxPassiveStrayCount: " << pol->TxPassiveStrayCount << std::endl;
  ss << "TxPassiveStrayOov[14]: " << pol->TxPassiveStrayOov << std::endl;
  ss << "TxPassiveStray[1052]: " << pol->TxPassiveStray << std::endl;
  ss << "BxHandleCount: " << pol->BxHandleCount << std::endl;
  ss << "BxHandles[NDI_MAX_HANDLES]: " << pol->BxHandles << std::endl;
  ss << "BxHandlesStatus[NDI_MAX_HANDLES]: " << pol->BxHandlesStatus << std::endl;
  ss << "BxFrameNumber[NDI_MAX_HANDLES]: " << pol->BxFrameNumber << std::endl;
  for (int i = 0; i < pol->BxHandleCount; ++i)
  {
    ss << "BxTransforms[" << i << "][8]: " << pol->BxTransforms[i] << std::endl;
  }
  ss << "BxPortStatus[NDI_MAX_HANDLES]: " << pol->BxPortStatus << std::endl;
  for (int i = 0; i < pol->BxHandleCount; ++i)
  {
    ss << "BxToolMarkerInformation[" << i << "][11]: " << pol->BxToolMarkerInformation[i] << std::endl;
  }
  ss << "BxActiveSingleStrayMarkerStatus[NDI_MAX_HANDLES]: " << pol->BxActiveSingleStrayMarkerStatus << std::endl;
  for (int i = 0; i < pol->BxHandleCount; ++i)
  {
    ss << "BxActiveSingleStrayMarkerPosition[" << i << "][3]: " << pol->BxActiveSingleStrayMarkerPosition[i] << std::endl;
  }
  ss << "Bx3DMarkerCount[NDI_MAX_HANDLES]: " << pol->Bx3DMarkerCount << std::endl;
  for (int i = 0; i < pol->BxHandleCount; ++i)
  {
    ss << "Bx3DMarkerOutOfVolume[" << i << "][3]: " << pol->Bx3DMarkerOutOfVolume[i] << std::endl;
  }
  for (int i = 0; i < pol->BxHandleCount; ++i)
  {
    for (int j = 0; j < pol->Bx3DMarkerCount[i]; j++)
    {
      ss << "Bx3DMarkerPosition[" << i << "][" << j << "][3]: " << pol->Bx3DMarkerPosition[i][j] << std::endl;
    }
  }
  ss << "BxPassiveStrayCount: " << pol->BxPassiveStrayCount << std::endl;
  ss << "BxPassiveStrayOutOfVolume[30]: " << pol->BxPassiveStrayOutOfVolume << std::endl;
  for (int i = 0; i < pol->BxPassiveStrayCount; ++i)
  {
    ss << "BxPassiveStrayPosition[" << i << "][3]: " << pol->BxPassiveStrayPosition[i] << std::endl;
  }

  assert(ss.str().size() < USHRT_MAX);
  strncpy(&outInformation[0], ss.str().c_str(), ss.str().size());
#else
  // Someone else can implement c style string building
#endif
}

//----------------------------------------------------------------------------
ndicapiExport void ndiTimeoutSocket(ndicapi* pol, int timeoutMsec)
{
  ndiSocketTimeout(pol->Socket, timeoutMsec);
}

//----------------------------------------------------------------------------
ndicapiExport NDIErrorCallback ndiGetErrorCallback(ndicapi* pol)
{
  return pol->ErrorCallback;
}

//----------------------------------------------------------------------------
ndicapiExport void* ndiGetErrorCallbackData(ndicapi* pol)
{
  return pol->ErrorCallbackData;
}

//----------------------------------------------------------------------------
ndicapiExport unsigned long ndiHexToUnsignedLong(const char* string, int n)
{
  int i;
  unsigned long result = 0;
  int c;

  for (i = 0; i < n; i++)
  {
    c = string[i];
    if (c >= 'a' && c <= 'f')
    {
      result = (result << 4) | (c + (10 - 'a'));
    }
    else if (c >= 'A' && c <= 'F')
    {
      result = (result << 4) | (c + (10 - 'A'));
    }
    else if (c >= '0' && c <= '9')
    {
      result = (result << 4) | (c - '0');
    }
    else
    {
      break;
    }
  }

  return result;
}

//----------------------------------------------------------------------------
ndicapiExport unsigned int ndiHexToUnsignedInt(const char* string, int n)
{
  int i;
  unsigned int result = 0;
  int c;

  for (i = 0; i < n; i++)
  {
    c = string[i];
    if (c >= 'a' && c <= 'f')
    {
      result = (result << 4) | (c + (10 - 'a'));
    }
    else if (c >= 'A' && c <= 'F')
    {
      result = (result << 4) | (c + (10 - 'A'));
    }
    else if (c >= '0' && c <= '9')
    {
      result = (result << 4) | (c - '0');
    }
    else
    {
      break;
    }
  }

  return result;
}

//----------------------------------------------------------------------------
ndicapiExport long ndiSignedToLong(const char* cp, int n)
{
  int i;
  int c;
  long result = 0;
  int sign = 1;

  c = cp[0];

  if (c == '+')
  {
    sign = 1;
  }
  else if (c == '-')
  {
    sign = -1;
  }
  else
  {
    return 0;
  }

  for (i = 1; i < n; i++)
  {
    c = cp[i];
    if (c >= '0' && c <= '9')
    {
      result = (result * 10) + (c - '0');
    }
    else
    {
      break;
    }
  }

  return sign * result;
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiHexEncode(char* cp, const void* data, int n)
{
  const unsigned char* bdata;
  int i, c1, c2;
  unsigned int d;
  char* tcp;

  bdata = (const unsigned char*)data;
  tcp = cp;

  for (i = 0; i < n; i++)
  {
    d = bdata[i];
    c1 = (d & 0xf0) >> 4;
    c2 = (d & 0x0f);
    c1 += '0';
    c2 += '0';
    if (c1 > '9')
    {
      c1 += ('A' - '0' - 10);
    }
    if (c2 > '9')
    {
      c2 += ('A' - '0' - 10);
    }
    *tcp++ = c1;
    *tcp++ = c2;
  }

  return cp;
}

//----------------------------------------------------------------------------
ndicapiExport void* ndiHexDecode(void* data, const char* cp, int n)
{
  unsigned char* bdata;
  int i, c1, c2;
  unsigned int d;

  bdata = (unsigned char*)data;

  for (i = 0; i < n; i++)
  {
    d = 0;
    c1 = *cp++;
    if (c1 >= 'a' && c1 <= 'f')
    {
      d = (c1 + (10 - 'a'));
    }
    else if (c1 >= 'A' && c1 <= 'F')
    {
      d = (c1 + (10 - 'A'));
    }
    else if (c1 >= '0' && c1 <= '9')
    {
      d = (c1 - '0');
    }
    c2 = *cp++;
    d <<= 4;
    if (c2 >= 'a' && c2 <= 'f')
    {
      d |= (c2 + (10 - 'a'));
    }
    else if (c2 >= 'A' && c2 <= 'F')
    {
      d |= (c2 + (10 - 'A'));
    }
    else if (c2 >= '0' && c2 <= '9')
    {
      d |= (c2 - '0');
    }

    bdata[i] = d;
  }

  return data;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiPVWRFromFile(ndicapi* pol, int port, char* filename)
{
  unsigned char buffer[1024];
  char hexdata[128];
  FILE* file;
  int addr;

  pol->ErrorCode = 0;

  file = fopen(filename, "rb");
  if (file == NULL)
  {
    return -1;
  }

  memset(buffer, 0, 1024);      // clear buffer to zero
  fread(buffer, 1, 1024, file); // read at most 1k from file
  if (ferror(file))
  {
    fclose(file);
    return -1;
  }
  fclose(file);

  for (addr = 0; addr < 1024; addr += 64)   // write in chunks of 64 bytes
  {
    ndiPVWR(pol, port, addr, ndiHexEncode(hexdata, &buffer[addr], 64));
    if (ndiGetError(pol) != NDI_OKAY)
    {
      return -1;
    }
  }

  return 0;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetError(ndicapi* pol)
{
  return pol->ErrorCode;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetSocketError(ndicapi* pol)
{
  return pol->SocketErrorCode;
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiErrorString(int errnum)
{
  static char* textarray_low[] = // values from 0x01 to 0x21
  {
    "No error",
    "Invalid command",
    "Command too long",
    "Command too short",
    "Invalid CRC calculated for command",
    "Time-out on command execution",
    "Unable to set up new communication parameters",
    "Incorrect number of command parameters",
    "Invalid port handle selected",
    "Invalid tracking priority selected (must be S, D or B)",
    "Invalid LED selected",
    "Invalid LED state selected (must be B, F or S)",
    "Command is invalid while in the current mode",
    "No tool assigned to the selected port handle",
    "Selected port handle not initialized",
    "Selected port handle not enabled",
    "System not initialized",
    "Unable to stop tracking",
    "Unable to start tracking",
    "Unable to initialize Tool-in-Port",
    "Invalid Position Sensor or Field Generator characterization parameters",
    "Unable to initialize the Measurement System",
    "Unable to start diagnostic mode",
    "Unable to stop diagnostic mode",
    "Unable to determine environmental infrared or magnetic interference",
    "Unable to read device's firmware version information",
    "Internal Measurement System error",
    "Unable to initialize for environmental infrared diagnostics",
    "Unable to set marker firing signature",
    "Unable to search for SROM IDs",
    "Unable to read SROM data",
    "Unable to write SROM data",
    "Unable to select SROM",
    "Unable to perform tool current test",
    "Unable to find camera parameters from the selected volume for the wavelength of a tool enabled for tracking",
    "Command parameter out of range",
    "Unable to select parameters by volume",
    "Unable to determine Measurement System supported features list",
    "Reserved - Unrecognized Error 0x26",
    "Reserved - Unrecognized Error 0x27",
    "SCU hardware has changed state; a card has been removed or added",
    "Main processor firmware corrupt",
    "No memory available for dynamic allocation (heap is full)",
    "Requested handle has not been allocated",
    "Requested handle has become unoccupied",
    "All handles have been allocated",
    "Invalid port description",
    "Requested port already assigned to a port handle",
    "Invalid input or output state",
  };

  static char* textarray_high[] = // values from 0xf6 to 0xf4
  {
    "Too much environmental infrared",
    "Unrecognized error code",
    "Unrecognized error code",
    "Unable to read Flash EPROM",
    "Unable to write Flash EPROM",
    "Unable to erase Flash EPROM"
  };

  static char* textarray_api[] = // values specific to the API
  {
    "Bad CRC on reply from Measurement System",
    "Error opening serial connection",
    "Host not capable of given communications parameters",
    "Device->host communication timeout",
    "Serial port write error",
    "Serial port read error",
    "Measurement System failed to reset on break",
    "Measurement System not found on specified port"
  };

  if (errnum >= 0x00 && errnum <= 0x31)
  {
    return textarray_low[errnum];
  }
  else if (errnum <= 0xf6 && errnum >= 0xf1)
  {
    return textarray_high[errnum - 0xf1];
  }
  else if (errnum >= 0x100 && errnum <= 0x700)
  {
    return textarray_api[(errnum >> 8) - 1];
  }

  return "Unrecognized error code";
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiSerialDeviceName(int i)
{
#if defined(_WIN32)

  // Windows
  const size_t MAX_BUF_LEN = 100;
  static char deviceName[100 + 1];
  char deviceNumber[100 + 1];
  int comPortNumber;
  comPortNumber = i + 1;
  deviceName[MAX_BUF_LEN] = 0;
  deviceNumber[MAX_BUF_LEN] = 0;
  if (comPortNumber < 1)
  {
    return NULL;
  }
  // COM port name format is different for port number under/over 10 (see Microsoft KB115831)
  if (comPortNumber < 10)
  {
    strcpy_s(deviceName, MAX_BUF_LEN, "COM");
  }
  else
  {
    strcpy_s(deviceName, MAX_BUF_LEN, "\\\\.\\COM");
  }
  _itoa_s(comPortNumber, deviceNumber, MAX_BUF_LEN, 10);
  strcat_s(deviceName, MAX_BUF_LEN, deviceNumber);
  return deviceName;

#elif defined (__APPLE__)

  // Apple
  static char devicenames[4][255 + 6];
  DIR* dirp;
  struct dirent* ep;
  int j = 0;

  dirp = opendir("/dev/");
  if (dirp == NULL)
  {
    return NULL;
  }

  while ((ep = readdir(dirp)) != NULL && j < 4)
  {
    if (ep->d_name[0] == 'c' && ep->d_name[1] == 'u' &&
        ep->d_name[2] == '.')
    {
      if (j == i)
      {
        strncpy(devicenames[j], "/dev/", 5);
        strncpy(devicenames[j] + 5, ep->d_name, 255);
        devicenames[j][255 + 5] == '\0';
        closedir(dirp);
        return devicenames[j];
      }
      j++;
    }
  }
  closedir(dirp);
  return NULL;

#else

  // Linux/Unix variants

#ifdef NDI_DEVICE0
  if (i == 0) { return NDI_DEVICE0; }
#endif
#ifdef NDI_DEVICE1
  if (i == 1) { return NDI_DEVICE1; }
#endif
#ifdef NDI_DEVICE2
  if (i == 2) { return NDI_DEVICE2; }
#endif
#ifdef NDI_DEVICE3
  if (i == 3) { return NDI_DEVICE3; }
#endif
#ifdef NDI_DEVICE4
  if (i == 4) { return NDI_DEVICE4; }
#endif
#ifdef NDI_DEVICE5
  if (i == 5) { return NDI_DEVICE5; }
#endif
#ifdef NDI_DEVICE6
  if (i == 6) { return NDI_DEVICE6; }
#endif
#ifdef NDI_DEVICE7
  if (i == 7) { return NDI_DEVICE7; }
#endif

  return NULL;

#endif
}

//----------------------------------------------------------------------------
ndicapiExport int ndiSerialProbe(const char* device)
{
  char reply[1024];
  char init_reply[16];
  NDIFileHandle serial_port;
  int n;

  serial_port = ndiSerialOpen(device);
  if (serial_port == NDI_INVALID_HANDLE)
  {
    return NDI_OPEN_ERROR;
  }

  // check DSR line to see whether any device is connected
  if (!ndiSerialCheckDSR(serial_port))
  {
    ndiSerialClose(serial_port);
    return NDI_PROBE_FAIL;
  }

  // set comm parameters to default, but decrease timeout to 0.1s
  if (ndiSerialComm(serial_port, 9600, "8N1", 0) < 0 || ndiSerialTimeout(serial_port, 100) < 0)
  {
    ndiSerialClose(serial_port);
    return NDI_BAD_COMM;
  }

  // flush the buffers (which are unlikely to contain anything)
  ndiSerialFlush(serial_port, NDI_IOFLUSH);

  // try to initialize ndicapi
  if (ndiSerialWrite(serial_port, "INIT:E3A5\r", 10) < 10 || ndiSerialSleep(serial_port, 100) < 0 ||
      ndiSerialRead(serial_port, init_reply, 16, false) <= 0 || strncmp(init_reply, "OKAYA896\r", 9) != 0)
  {
    // increase timeout to 5 seconds for reset
    ndiSerialTimeout(serial_port, 5000);

    // init failed: flush, reset, and try again
    ndiSerialFlush(serial_port, NDI_IOFLUSH);
    if (ndiSerialFlush(serial_port, NDI_IOFLUSH) < 0 ||
        ndiSerialBreak(serial_port))
    {
      ndiSerialClose(serial_port);
      return NDI_BAD_COMM;
    }

    n = ndiSerialRead(serial_port, init_reply, 16, false);
    if (n < 0)
    {
      ndiSerialClose(serial_port);
      return NDI_READ_ERROR;
    }
    else if (n == 0)
    {
      ndiSerialClose(serial_port);
      return NDI_TIMEOUT;
    }

    // check reply from reset
    if (strncmp(init_reply, "RESETBE6F\r", 10) != 0)
    {
      ndiSerialClose(serial_port);
      return NDI_PROBE_FAIL;
    }
    // try to initialize a second time
    ndiSerialSleep(serial_port, 100);
    n = ndiSerialWrite(serial_port, "INIT:E3A5\r", 10);
    if (n < 0)
    {
      ndiSerialClose(serial_port);
      return NDI_WRITE_ERROR;
    }
    else if (n < 10)
    {
      ndiSerialClose(serial_port);
      return NDI_TIMEOUT;
    }

    ndiSerialSleep(serial_port, 100);
    n = ndiSerialRead(serial_port, init_reply, 16, false);
    if (n < 0)
    {
      ndiSerialClose(serial_port);
      return NDI_READ_ERROR;
    }
    else if (n == 0)
    {
      ndiSerialClose(serial_port);
      return NDI_TIMEOUT;
    }

    if (strncmp(init_reply, "OKAYA896\r", 9) != 0)
    {
      ndiSerialClose(serial_port);
      return NDI_PROBE_FAIL;
    }
  }

  ndiSerialSleep(serial_port, 100);
  // Example exchange with Polaris Vicra
  //>> GETINFO:Features.Firmware.Version0492
  //<< Features.Firmware.Version=007.000.012;3;1;0;12;;Current firmware revision number99A8
  if (ndiSerialWrite(serial_port, "GETINFO:Features.Firmware.Version0492\r", 10) < 10 ||
      (n = ndiSerialRead(serial_port, reply, 1023, false)) < 84)
  {
    ndiSerialClose(serial_port);
    return NDI_PROBE_FAIL;
  }

  // restore things back to the way they were
  ndiSerialClose(serial_port);

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport ndicapi* ndiOpenSerial(const char* device)
{
  NDIFileHandle serial_port;
  ndicapi* pol;

  serial_port = ndiSerialOpen(device);

  if (serial_port == NDI_INVALID_HANDLE)
  {
    return NULL;
  }

  if (ndiSerialComm(serial_port, 9600, "8N1", 0) < 0)
  {
    ndiSerialClose(serial_port);
    return NULL;
  }

  if (ndiSerialFlush(serial_port, NDI_IOFLUSH) < 0)
  {
    ndiSerialClose(serial_port);
    return NULL;
  }

  pol = (ndicapi*)malloc(sizeof(ndicapi));

  if (pol == 0)
  {
    ndiSerialClose(serial_port);
    return NULL;
  }

  memset(pol, 0, sizeof(ndicapi));
  pol->SerialDevice = serial_port;

  // allocate the buffers
  pol->SerialDeviceName = (char*)malloc(strlen(device) + 1);
  pol->Command = (char*)malloc(2048);
  pol->Reply = (char*)malloc(2048);
  pol->ReplyNoCRC = (char*)malloc(2048);
  pol->Hostname = NULL;
  pol->Port = -1;
  pol->Socket = -1;

  if (pol->SerialDeviceName == 0 || pol->Command == 0 || pol->Reply == 0 || pol->ReplyNoCRC == 0)
  {
    if (pol->SerialDeviceName)
    {
      free(pol->SerialDeviceName);
    }
    if (pol->Command)
    {
      free(pol->Command);
    }
    if (pol->Reply)
    {
      free(pol->Reply);
    }
    if (pol->ReplyNoCRC)
    {
      free(pol->ReplyNoCRC);
    }

    ndiSerialClose(serial_port);
    return NULL;
  }

  // initialize the allocated memory
  strcpy(pol->SerialDeviceName, device);
  memset(pol->Command, 0, 2048);
  memset(pol->Reply, 0, 2048);
  memset(pol->ReplyNoCRC, 0, 2048);

  return pol;
}

//----------------------------------------------------------------------------
ndicapiExport ndicapi* ndiOpenNetwork(const char* hostname, int port)
{
  NDISocketHandle socket;
  ndicapi* device;

  int connected = ndiSocketOpen(hostname, port, socket);

  if (socket == -1 || connected == -1)
  {
    return NULL;
  }

  device = (ndicapi*)malloc(sizeof(ndicapi));

  if (device == 0)
  {
    ndiSocketClose(socket);
    return NULL;
  }

  memset(device, 0, sizeof(ndicapi));
  device->Hostname = new char[strlen(hostname) + 1];
  device->Hostname = strncpy(device->Hostname, hostname, strlen(hostname));
  device->Port = port;
  device->Socket = socket;
  device->SerialDevice = NDI_INVALID_HANDLE;
  device->SerialDeviceName = NULL;

  // allocate the buffers
  device->Command = (char*)malloc(2048);
  device->Reply = (char*)malloc(2048);
  device->ReplyNoCRC = (char*)malloc(2048);

  // initialize the allocated memory
  memset(device->Command, 0, 2048);
  memset(device->Reply, 0, 2048);
  memset(device->ReplyNoCRC, 0, 2048);

  return device;
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiGetSerialDeviceName(ndicapi* pol)
{
  return pol->SerialDeviceName;
}

//----------------------------------------------------------------------------
ndicapiExport NDISocketHandle ndiGetSocket(ndicapi* pol)
{
  return pol->Socket;
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiGetHostname(ndicapi* pol)
{
  return pol->Hostname;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPort(ndicapi* pol)
{
  return pol->Port;
}

//----------------------------------------------------------------------------
ndicapiExport NDIFileHandle ndiGetDeviceHandle(ndicapi* pol)
{
  return pol->SerialDevice;
}

//----------------------------------------------------------------------------
ndicapiExport void ndiCloseSerial(ndicapi* device)
{
  // end the tracking thread if it is running
  ndiSetThreadMode(device, 0);

  // close the serial port
  ndiSerialClose(device->SerialDevice);

  // free the buffers
  free(device->SerialDeviceName);
  free(device->Command);
  free(device->Reply);
  free(device->ReplyNoCRC);
  device->SerialDeviceName = NULL;
  device->SerialDevice = NDI_INVALID_HANDLE;

  free(device);
}

//----------------------------------------------------------------------------
ndicapiExport void ndiCloseNetwork(ndicapi* device)
{
  // end the tracking thread if it is running
  ndiSetThreadMode(device, 0);

  // close the serial port
  ndiSocketClose(device->Socket);

  // free the buffers
  free(device->Hostname);
  free(device->Command);
  free(device->Reply);
  free(device->ReplyNoCRC);
  device->Hostname = NULL;
  device->Port = -1;
  device->Socket = -1;

  free(device);
}

//----------------------------------------------------------------------------
// the CalcCRC16 function is taken from the NDI ndicapi documentation
//----------------------------------------------------------------------------*/
// Name:                   CalcCRC16
//
// Input Values:
//     int
//         data        :Data value to add to running CRC16.
//     unsigned int
//         *puCRC16    :Ptr. to running CRC16.
//
// Output Values:
//     None.
//
// Returned Value:
//     None.
//
// Description:
//     This routine calculates a running CRC16 using the polynomial
//     X^16 + X^15 + X^2 + 1.
//
static const int oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1,
                                   1, 0, 0, 1, 0, 1, 1, 0
                                 };

#define CalcCRC16(nextchar, puCRC16) \
{ \
    int data; \
    data = nextchar; \
    data = (data ^ (*(puCRC16) & 0xff)) & 0xff; \
    *puCRC16 >>= 8; \
    if ( oddparity[data & 0x0f] ^ oddparity[data >> 4] ) { \
      *(puCRC16) ^= 0xc001; \
    } \
    data <<= 6; \
    *puCRC16 ^= data; \
    data <<= 1; \
    *puCRC16 ^= data; \
}

//----------------------------------------------------------------------------
// Prototypes for helper functions for certain commands.  These functions
//   are called whenever the corresponding command sent the Measurement System
//   unless an error was generated.
//
//   cp  -> the command string that was sent to the NDICAPI
//   crp -> the reply from the Measurement System, but with the CRC hacked off

namespace
{
  //----------------------------------------------------------------------------
  // Copy all the PHINF reply information into the ndicapi structure, according
  // to the PHINF reply mode that was requested.
  //
  // This function is called every time a PHINF command is sent to the
  // Measurement System.
  //
  // This information can be later extracted through one of the ndiGetPHINFxx()
  // functions.
  void ndiPHINFHelper(ndicapi* pol, const char* cp, const char* crp)
  {
    unsigned long mode = 0x0001; // the default reply mode
    char* dp;
    int j;
    int unoccupied = NDI_OKAY;

    // if the PHINF command had a reply mode, read it
    if ((cp[5] == ':' && cp[10] != '\r') || (cp[5] == ' ' && cp[6] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&cp[8], 4);
    }

    // check for unoccupied
    if (*crp == 'U')
    {
      unoccupied = NDI_UNOCCUPIED;
    }
    pol->PhinfUnoccupied = unoccupied;

    // fprintf(stderr, "mode = %04lx\n", mode);

    if (mode & NDI_BASIC)
    {
      dp = pol->PhinfBasic;
      if (!unoccupied)
      {
        for (j = 0; j < 33 && *crp >= ' '; j++)
        {
          // fprintf(stderr,"%c",*crp);
          *dp++ = *crp++;
        }
        // fprintf(stderr,"\n");
      }
      else    // default "00000000            0000000000000"
      {
        for (j = 0; j < 8; j++)
        {
          *dp++ = '0';
        }
        for (j = 0; j < 12; j++)
        {
          *dp++ = ' ';
        }
        for (j = 0; j < 13; j++)
        {
          *dp++ = '0';
        }
      }
    }

    if (mode & NDI_TESTING)
    {
      dp = pol->PhinfTesting;
      if (!unoccupied)
      {
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }
      else   // default "00000000"
      {
        for (j = 0; j < 8; j++)
        {
          *dp++ = '0';
        }
      }
    }

    if (mode & NDI_PART_NUMBER)
    {
      dp = pol->PhinfPartNumber;
      if (!unoccupied)
      {
        for (j = 0; j < 20 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }
      else   // default "                    "
      {
        for (j = 0; j < 20; j++)
        {
          *dp++ = ' ';
        }
      }
    }

    if (mode & NDI_ACCESSORIES)
    {
      dp = pol->PhinfAccessories;
      if (!unoccupied)
      {
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }
      else   // default "00"
      {
        for (j = 0; j < 2; j++)
        {
          *dp++ = '0';
        }
      }
    }

    if (mode & NDI_MARKER_TYPE)
    {
      dp = pol->PhinfMarkerType;
      if (!unoccupied)
      {
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }
      else   // default "00"
      {
        for (j = 0; j < 2; j++)
        {
          *dp++ = '0';
        }
      }
    }

    if (mode & NDI_PORT_LOCATION)
    {
      dp = pol->PhinfPortLocation;
      if (!unoccupied)
      {
        for (j = 0; j < 14 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }
      else   // default "00000000000000"
      {
        for (j = 0; j < 14; j++)
        {
          *dp++ = '0';
        }
      }
    }

    if (mode & NDI_GPIO_STATUS)
    {
      dp = pol->PhinfGpioStatus;
      if (!unoccupied)
      {
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }
      else   // default "00"
      {
        for (j = 0; j < 2; j++)
        {
          *dp++ = '0';
        }
      }
    }
  }

  //----------------------------------------------------------------------------
  //Copy all the PHRQ reply information into the ndicapi structure.
  //
  //This function is called every time a PHRQ command is sent to the
  //Measurement System.
  //
  //This information can be later extracted through one of the ndiGetPHRQHandle()
  //functions.
  void ndiPHRQHelper(ndicapi* pol, const char* cp, const char* crp)
  {
    char* dp;
    int j;

    dp = pol->PhrqReply;
    for (j = 0; j < 2; j++)
    {
      *dp++ = *crp++;
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the PHSR reply information into the ndicapi structure.
  //
  // This function is called every time a PHSR command is sent to the
  // Measurement System.
  //
  // This information can be later extracted through one of the ndiGetPHSRxx()
  // functions.
  void ndiPHSRHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    char* writePointer;
    int j;

    writePointer = pol->PhsrReply;
    for (j = 0; j < 1282 && *commandReply >= ' '; j++)
    {
      *writePointer++ = *commandReply++;
    }
    *writePointer++ = '\0';
  }

  //----------------------------------------------------------------------------
  // Copy all the TX reply information into the ndicapi structure, according
  // to the TX reply mode that was requested.
  //
  // This function is called every time a TX command is sent to the
  // Measurement System.
  //
  // This information can be later extracted through one of the ndiGetTXxx()
  // functions.
  void ndiTXHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    unsigned long mode = NDI_XFORMS_AND_STATUS; // the default reply mode
    char* writePointer;
    int i, j, n;
    int handle, handleCount, strayCount;

    // if the TX command had a reply mode, read it
    if ((command[2] == ':' && command[7] != '\r') || (command[2] == ' ' && command[3] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&command[3], 4);
    }

    // get the number of handles
    handleCount = (int)ndiHexToUnsignedLong(commandReply, 2);
    for (j = 0; j < 2 && *commandReply >= ' '; j++)
    {
      commandReply++;
    }

    // go through the information for each handle
    for (i = 0; i < handleCount; i++)
    {
      // get the handle itself (two chars)
      handle = (int)ndiHexToUnsignedLong(commandReply, 2);
      for (j = 0; j < 2 && *commandReply >= ' '; j++)
      {
        commandReply++;
      }

      // check for "UNOCCUPIED"
      if (*commandReply == 'U')
      {
        for (j = 0; j < 10 && *commandReply >= ' '; j++)
        {
          commandReply++;
        }
        // back up and continue (don't store information for unoccupied ports)
        i--;
        handleCount--;
        continue;
      }

      // save the port handle in the list
      pol->TxHandles[i] = handle;

      if (mode & NDI_XFORMS_AND_STATUS)
      {
        // get the transform, MISSING, or DISABLED
        writePointer = pol->TxTransforms[i];

        if (*commandReply == 'M')
        {
          // check for "MISSING"
          for (j = 0; j < 7 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
        }
        else if (*commandReply == 'D')
        {
          // check for "DISABLED"
          for (j = 0; j < 8 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
        }
        else
        {
          // read the transform
          for (j = 0; j < 51 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
        }
        *writePointer = '\0';

        // get the status
        writePointer = pol->TxStatus[i];
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }

        // get the frame number
        writePointer = pol->TxFrame[i];
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // grab additional information
      if (mode & NDI_ADDITIONAL_INFO)
      {
        writePointer = pol->TxInformation[i];
        for (j = 0; j < 20 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // grab the single marker info
      if (mode & NDI_SINGLE_STRAY)
      {
        writePointer = pol->TxSingleStray[i];
        if (*commandReply == 'M')
        {
          // check for "MISSING"
          for (j = 0; j < 7 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
        }
        else if (*commandReply == 'D')
        {
          // check for "DISABLED"
          for (j = 0; j < 8 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
        }
        else
        {
          // read the single stray position
          for (j = 0; j < 21 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
        }
        *writePointer = '\0';
      }

      // skip over any unsupported information
      while (*commandReply >= ' ')
      {
        commandReply++;
      }

      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    // save the number of handles (minus the unoccupied handles)
    pol->TxHandleCount = handleCount;

    // get all the passive stray information
    // this will be a maximum of 3 + 13 + 50*3*7 = 1066 bytes
    if (mode & NDI_PASSIVE_STRAY)
    {
      // get the number of strays
      strayCount = (int)ndiSignedToLong(commandReply, 3);
      for (j = 0; j < 2 && *commandReply >= ' '; j++)
      {
        commandReply++;
      }
      if (strayCount > 50)
      {
        strayCount = 50;
      }
      pol->TxPassiveStrayCount = strayCount;
      // get the out-of-volume bits
      writePointer = pol->TxPassiveStrayOov;
      n = (strayCount + 3) / 4;
      for (j = 0; j < n && *commandReply >= ' '; j++)
      {
        *writePointer++ = *commandReply++;
      }
      // get the coordinates
      writePointer = pol->TxPassiveStray;
      n = strayCount * 21;
      for (j = 0; j < n && *commandReply >= ' '; j++)
      {
        *writePointer++ = *commandReply++;
      }
      *writePointer = '\0';
    }

    // get the system status
    writePointer = pol->TxSystemStatus;
    for (j = 0; j < 4 && *commandReply >= ' '; j++)
    {
      *writePointer++ = *commandReply++;
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the BX reply information into the ndicapi structure, according
  // to the BX reply mode that was requested.
  //
  // This function is called every time a BX command is sent to the Measurement System.
  //
  // This information can be later extracted through one of the ndiGetTXxx()
  // functions.
  void ndiBXHelper(ndicapi* api, const char* command, const char* commandReply)
  {
    // Reply options
    // NDI_XFORMS_AND_STATUS  0x0001  /* transforms and status */
    // NDI_ADDITIONAL_INFO    0x0002  /* additional tool transform info */
    // NDI_SINGLE_STRAY       0x0004  /* stray active marker reporting */
    // NDI_FRAME_NUMBER       0x0008  /* frame number for each tool */
    // NDI_PASSIVE            0x8000  /* report passive tool information */
    // NDI_PASSIVE_EXTRA      0x2000  /* add 6 extra passive tools */
    // NDI_PASSIVE_STRAY      0x1000  /* stray passive marker reporting */
    unsigned long mode = NDI_XFORMS_AND_STATUS;
    const char* replyIndex;
    unsigned short replyLength;
    unsigned short headerCRC;

    // if the BX command had a reply option, read it
    if ((command[2] == ':' && command[7] != '\r') || (command[2] == ' ' && command[3] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&command[3], 4);
    }

    replyIndex = &commandReply[0];

    // Confirm start sequence
    if (replyIndex[0] != (char)0xc4 || replyIndex[1] != (char)0xa5)  // little endian
    {
      // Something isn't right, abort
      return;
    }
    replyIndex += 2;

    // Get the reply length
    replyLength = (unsigned char)replyIndex[1] << 8 | (unsigned char)replyIndex[0];
    replyIndex += 2;

    // Get the CRC
    headerCRC = (unsigned char)replyIndex[1] << 8 | (unsigned char)replyIndex[0];
    replyIndex += 2;

    // Get the number of handles
    api->BxHandleCount = (unsigned char)replyIndex[0];
    replyIndex += 1;

    // Go through the information for each handle
    for (unsigned short i = 0; i < api->BxHandleCount; i++)
    {
      // get the handle itself
      api->BxHandles[i] = (char)replyIndex[0];
      replyIndex++;

      api->BxHandlesStatus[i] = (char)replyIndex[0];
      replyIndex++;

      // Disabled handles have no reply data
      if (api->BxHandlesStatus[i] == NDI_HANDLE_DISABLED)
      {
        continue;
      }

      if (mode & NDI_XFORMS_AND_STATUS)
      {
        if (api->BxHandlesStatus[i] != NDI_HANDLE_MISSING)
        {
          // 4 float, Q0, Qx, Qy, Qz
          api->BxTransforms[i][0] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxTransforms[i][1] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxTransforms[i][2] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxTransforms[i][3] = *(float*)replyIndex;
          replyIndex += 4;

          // 3 float, Tx, Ty, Tz
          api->BxTransforms[i][4] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxTransforms[i][5] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxTransforms[i][6] = *(float*)replyIndex;
          replyIndex += 4;

          // 1 float, RMS error
          api->BxTransforms[i][7] = *(float*)replyIndex;
          replyIndex += 4;
        }
        // 4 bytes port status
        api->BxPortStatus[i] = (int)replyIndex[0] | (int)replyIndex[1] << 8 | (int)replyIndex[2] << 16 | (int)replyIndex[3] << 24;
        replyIndex += 4;
        // 4 bytes frame number
        api->BxFrameNumber[i] = (unsigned int)replyIndex[0] | (unsigned int)replyIndex[1] << 8 | (unsigned int)replyIndex[2] << 16 | (unsigned int)replyIndex[3] << 24;
        replyIndex += 4;
      }

      // grab additional information
      if (mode & NDI_ADDITIONAL_INFO)
      {
        api->BxToolMarkerInformation[i][0] = (char)replyIndex[0];
        replyIndex++;
        for (int j = 0; j < 10; j++)
        {
          api->BxToolMarkerInformation[i][j + 1] = (char)replyIndex[0];
          replyIndex++;
        }
      }

      // grab the single marker info
      if (mode & NDI_SINGLE_STRAY)
      {
        char activeStatus = (char)replyIndex[0];
        replyIndex++;
        api->BxActiveSingleStrayMarkerStatus[i] = activeStatus;

        if (activeStatus != 0x00 || (mode & NDI_NOT_NORMALLY_REPORTED && activeStatus & NDI_ACTIVE_STRAY_OUT_OF_VOLUME))
        {
          // Marker is not missing, or it is out-of-volume and not-normally-requested is requested
          // Either means we have data...
          // 3 float, Tx, Ty, Tz
          api->BxActiveSingleStrayMarkerPosition[i][0] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxActiveSingleStrayMarkerPosition[i][1] = *(float*)replyIndex;
          replyIndex += 4;
          api->BxActiveSingleStrayMarkerPosition[i][2] = *(float*)replyIndex;
          replyIndex += 4;
        }
      }

      if (mode & NDI_3D_MARKER_POSITIONS)
      {
        // Save marker count
        api->Bx3DMarkerCount[i] = (char)replyIndex[0];
        replyIndex++;

        // Save off out of volume status
        int numBytes = static_cast<int>(ceilf(api->Bx3DMarkerCount[i] / 8.f));
        for (int j = 0; j < numBytes; ++j)
        {
          api->Bx3DMarkerOutOfVolume[i][j] = (char)replyIndex[0];
          ++replyIndex;
        }

        for (int j = 0; j < api->Bx3DMarkerCount[i]; ++j)
        {
          // 3 float, Tx, Ty, Tz
          api->Bx3DMarkerPosition[i][j][0] = *(float*)replyIndex;
          replyIndex += 4;
          api->Bx3DMarkerPosition[i][j][1] = *(float*)replyIndex;
          replyIndex += 4;
          api->Bx3DMarkerPosition[i][j][2] = *(float*)replyIndex;
          replyIndex += 4;
        }
      }
    }

    if (mode & NDI_PASSIVE_STRAY)
    {
      // Save marker count
      api->BxPassiveStrayCount = (char)replyIndex[0];
      replyIndex++;

      if (api->BxPassiveStrayCount > 240)
      {
        // This implementation cannot report on more than 240 stray passive markers
        api->BxPassiveStrayCount = 240;
      }

      // Save off out of volume status
      int numBytes = static_cast<int>(ceilf(api->BxPassiveStrayCount / 8.f));
      for (int j = 0; j < numBytes; ++j)
      {
        api->BxPassiveStrayOutOfVolume[j] = (char)replyIndex[0];
        ++replyIndex;
      }

      for (int j = 0; j < api->BxPassiveStrayCount; ++j)
      {
        // 3 float, Tx, Ty, Tz
        api->BxPassiveStrayPosition[j][0] = *(float*)replyIndex;
        replyIndex += 4;
        api->BxPassiveStrayPosition[j][1] = *(float*)replyIndex;
        replyIndex += 4;
        api->BxPassiveStrayPosition[j][2] = *(float*)replyIndex;
        replyIndex += 4;
      }
    }

    // Get the system status
    api->BxSystemStatus = (char)replyIndex[1] << 8 | (char)replyIndex[0];
    replyIndex += 2;
  }

  //----------------------------------------------------------------------------
  // Copy all the GX reply information into the ndicapi structure, according
  // to the GX reply mode that was requested.
  //
  // This function is called every time a GX command is sent to the
  // Measurement System.
  //
  // This information can be later extracted through one of the ndiGetGXxx()
  // functions.
  void ndiGXHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    unsigned long mode = NDI_XFORMS_AND_STATUS; // the default reply mode
    char* writePointer;
    int i, j, k;
    int passiveCount, activeCount;

    // if the GX command had a reply mode, read it
    if ((command[2] == ':' && command[7] != '\r') || (command[2] == ' ' && command[3] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&command[3], 4);
    }

    // always three active ports
    activeCount = 3;

    if (mode & NDI_XFORMS_AND_STATUS)
    {
      for (k = 0; k < activeCount; k += 3)
      {
        // grab the three transforms
        for (i = 0; i < 3; i++)
        {
          writePointer = pol->GxTransforms[i];
          for (j = 0; j < 51 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
          *writePointer = '\0';
          // eat the trailing newline
          if (*commandReply == '\n')
          {
            commandReply++;
          }
        }
        // grab the status flags
        writePointer = pol->GxStatus + k / 3 * 8;
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }
      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    if (mode & NDI_ADDITIONAL_INFO)
    {
      // grab information for each port
      for (i = 0; i < activeCount; i++)
      {
        writePointer = pol->GxInformation[i];
        for (j = 0; j < 12 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }
      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    if (mode & NDI_SINGLE_STRAY)
    {
      // grab stray marker for each port
      for (i = 0; i < activeCount; i++)
      {
        writePointer = pol->GxSingleStray[i];
        for (j = 0; j < 21 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
        *writePointer = '\0';
        // eat the trailing newline
        if (*commandReply == '\n')
        {
          commandReply++;
        }
      }
    }

    if (mode & NDI_FRAME_NUMBER)
    {
      // get frame number for each port
      for (i = 0; i < activeCount; i++)
      {
        writePointer = pol->GxFrame[i];
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }
      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    // if there is no passive information, stop here
    if (!(mode & NDI_PASSIVE))
    {
      return;
    }

    // in case there are 9 passive tools instead of just 3
    passiveCount = 3;
    if (mode & NDI_PASSIVE_EXTRA)
    {
      passiveCount = 9;
    }

    if ((mode & NDI_XFORMS_AND_STATUS) || (mode == NDI_PASSIVE))
    {
      // the information is grouped in threes
      for (k = 0; k < passiveCount; k += 3)
      {
        // grab the three transforms
        for (i = 0; i < 3; i++)
        {
          writePointer = pol->GxPassiveTransforms[k + i];
          for (j = 0; j < 51 && *commandReply >= ' '; j++)
          {
            *writePointer++ = *commandReply++;
          }
          *writePointer = '\0';
          // eat the trailing newline
          if (*commandReply == '\n')
          {
            commandReply++;
          }
        }
        // grab the status flags
        writePointer = pol->GxPassiveStatus + k / 3 * 8;
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
        // skip the newline
        if (*commandReply == '\n')
        {
          commandReply++;
        }
        else   // no newline: no more passive transforms
        {
          passiveCount = k + 3;
        }
      }
      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    if (mode & NDI_ADDITIONAL_INFO)
    {
      // grab information for each port
      for (i = 0; i < passiveCount; i++)
      {
        writePointer = pol->GxPassiveInformation[i];
        for (j = 0; j < 12 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }
      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    if (mode & NDI_FRAME_NUMBER)
    {
      // get frame number for each port
      for (i = 0; i < passiveCount; i++)
      {
        writePointer = pol->GxPassiveFrame[i];
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }
      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    if (mode & NDI_PASSIVE_STRAY)
    {
      // get all the passive stray information
      // this will be a maximum of 3 + 20*3*7 = 423 bytes
      writePointer = pol->GxPassiveStray;
      for (j = 0; j < 423 && *commandReply >= ' '; j++)
      {
        *writePointer++ = *commandReply++;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the PSTAT reply information into the ndicapi structure.
  void ndiPSTATHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    unsigned long mode = NDI_XFORMS_AND_STATUS; // the default reply mode
    char* writePointer;
    int i, j;
    int passiveCount, activeCount;

    // if the PSTAT command had a reply mode, read it
    if ((command[5] == ':' && command[10] != '\r') || (command[5] == ' ' && command[6] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&command[6], 4);
    }

    // always three active ports
    activeCount = 3;

    // information for each port is separated by a newline
    for (i = 0; i < activeCount; i++)
    {
      // basic tool information and port status
      if (mode & NDI_BASIC)
      {
        writePointer = pol->PstatBasic[i];
        for (j = 0; j < 32 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
        // terminate if UNOCCUPIED
        if (j < 32)
        {
          *writePointer = '\0';
        }
      }

      // current testing
      if (mode & NDI_TESTING)
      {
        writePointer = pol->PstatTesting[i];
        *writePointer = '\0';
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // part number
      if (mode & NDI_PART_NUMBER)
      {
        writePointer = pol->PstatPartNumber[i];
        *writePointer = '\0';
        for (j = 0; j < 20 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // accessories
      if (mode & NDI_ACCESSORIES)
      {
        writePointer = pol->PstatAccessories[i];
        *writePointer = '\0';
        for (j = 0; j < 2 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // marker type
      if (mode & NDI_MARKER_TYPE)
      {
        writePointer = pol->PstatMarkerType[i];
        *writePointer = '\0';
        for (j = 0; j < 2 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // skip any other information that might be present
      while (*commandReply >= ' ')
      {
        commandReply++;
      }

      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }

    // if there is no passive information, stop here
    if (!(mode & NDI_PASSIVE))
    {
      return;
    }

    // in case there are 9 passive tools instead of just 3
    passiveCount = 3;
    if (mode & NDI_PASSIVE_EXTRA)
    {
      passiveCount = 9;
    }

    // information for each port is separated by a newline
    for (i = 0; i < passiveCount; i++)
    {
      // basic tool information and port status
      if (mode & NDI_BASIC)
      {
        writePointer = pol->PstatPassiveBasic[i];
        *writePointer = '\0';
        for (j = 0; j < 32 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
        // terminate if UNOCCUPIED
        if (j < 32)
        {
          *writePointer = '\0';
        }
      }

      // current testing
      if (mode & NDI_TESTING)
      {
        writePointer = pol->PstatPassiveTesting[i];
        *writePointer = '\0';
        for (j = 0; j < 8 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // part number
      if (mode & NDI_PART_NUMBER)
      {
        writePointer = pol->PstatPassivePartNumber[i];
        *writePointer = '\0';
        for (j = 0; j < 20 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // accessories
      if (mode & NDI_ACCESSORIES)
      {
        writePointer = pol->PstatPassiveAccessories[i];
        *writePointer = '\0';
        for (j = 0; j < 2 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // marker type
      if (mode & NDI_MARKER_TYPE)
      {
        writePointer = pol->PstatPassiveMarkerType[i];
        *writePointer = '\0';
        for (j = 0; j < 2 && *commandReply >= ' '; j++)
        {
          *writePointer++ = *commandReply++;
        }
      }

      // skip any other information that might be present
      while (*commandReply >= ' ')
      {
        commandReply++;
      }

      // eat the trailing newline
      if (*commandReply == '\n')
      {
        commandReply++;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the SSTAT reply information into the ndicapi structure.
  void ndiSSTATHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    unsigned long mode;
    char* writePointer;

    // read the SSTAT command reply mode
    mode = ndiHexToUnsignedLong(&command[6], 4);

    if (mode & NDI_CONTROL)
    {
      writePointer = pol->SstatControl;
      *writePointer++ = *commandReply++;
      *writePointer++ = *commandReply++;
    }

    if (mode & NDI_SENSORS)
    {
      writePointer = pol->SstatSensor;
      *writePointer++ = *commandReply++;
      *writePointer++ = *commandReply++;
    }

    if (mode & NDI_TIU)
    {
      writePointer = pol->SstatTiu;
      *writePointer++ = *commandReply++;
      *writePointer++ = *commandReply++;
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the IRCHK reply information into the ndicapi structure.
  void ndiIRCHKHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    unsigned long mode = NDI_XFORMS_AND_STATUS; // the default reply mode
    int j;

    // if the IRCHK command had a reply mode, read it
    if ((command[5] == ':' && command[10] != '\r') || (command[5] == ' ' && command[6] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&command[6], 4);
    }

    // a single character, '0' or '1'
    if (mode & NDI_DETECTED)
    {
      pol->IrchkDetected = *commandReply++;
    }

    // maximum string length for 20 sources is 2*(3 + 20*3) = 126
    // copy until a control char (less than 0x20) is found
    if (mode & NDI_SOURCES)
    {
      for (j = 0; j < 126 && *commandReply >= ' '; j++)
      {
        pol->IrchkSources[j] = *commandReply++;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Adjust the host to match a COMM command.
  void ndiCOMMHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    static int convert_baud[8] = { 9600, 14400, 19200, 38400, 57600, 115200, 921600, 1228739 };
    char newdps[4] = "8N1";
    int newspeed = 9600;
    int newhand = 0;

    if (command[5] >= '0' && command[5] <= '7')
    {
      newspeed = convert_baud[command[5] - '0'];
    }
    if (command[6] == '1')
    {
      newdps[0] = '7';
    }
    if (command[7] == '1')
    {
      newdps[1] = 'O';
    }
    else if (command[7] == '2')
    {
      newdps[1] = 'E';
    }
    if (command[8] == '1')
    {
      newdps[2] = '2';
    }
    if (command[9] == '1')
    {
      newhand = 1;
    }

    ndiSerialSleep(pol->SerialDevice, 100);  // let the device adjust itself
    if (ndiSerialComm(pol->SerialDevice, newspeed, newdps, newhand) != 0)
    {
      ndiSetError(pol, NDI_BAD_COMM);
    }
  }

  //----------------------------------------------------------------------------
  // Sleep for 100 milliseconds after an INIT command.
  void ndiINITHelper(ndicapi* pol, const char* command, const char* commandReply)
  {
    if (pol->SerialDevice != NDI_INVALID_HANDLE)
    {
      ndiSerialSleep(pol->SerialDevice, 100);
    }
    else
    {
      ndiSocketSleep(pol->Socket, 100);
    }
  }
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiCommand(ndicapi* pol, const char* format, ...)
{
  char* reply;
  va_list ap;            // see stdarg.h
  va_start(ap, format);

  reply = ndiCommandVA(pol, format, ap);

  va_end(ap);

  return reply;
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiCommandVA(ndicapi* api, const char* format, va_list ap)
{
  int i, bytes, commandLength;
  bool useCrc = false;
  bool inCommand = true;
  char* command;
  char* reply;
  char* commandReply;

  command = api->Command;       // text sent to ndicapi
  reply = api->Reply;     // text received from ndicapi
  commandReply = api->ReplyNoCRC;   // received text, with CRC hacked off
  commandLength = 0;                  // length of 'command' part of command

  api->ErrorCode = 0;                 // clear error
  command[0] = '\0';
  reply[0] = '\0';
  commandReply[0] = '\0';

  // verify that the serial device was opened
  if (api->SerialDevice == NDI_INVALID_HANDLE && api->Hostname == NULL && api->Port < 0)
  {
    ndiSetError(api, NDI_OPEN_ERROR);
    return commandReply;
  }

  // if the command is NULL, send a break to reset the Measurement System
  if (format == NULL)
  {
    if (api->IsThreadedMode && api->IsTracking)
    {
      // block the tracking thread
      ndiMutexLock(api->ThreadMutex);
    }
    api->IsTracking = false;

    if (api->SerialDevice != NDI_INVALID_HANDLE)
    {
      ndiSerialComm(api->SerialDevice, 9600, "8N1", 0);
      ndiSerialFlush(api->SerialDevice, NDI_IOFLUSH);
      ndiSerialBreak(api->SerialDevice);
      bytes = ndiSerialRead(api->SerialDevice, reply, 2047, false);
    }
    else
    {
      int errorCode;
      bytes = ndiSocketRead(api->Socket, reply, 2047, false, &errorCode);
    }

    // check for correct reply
    if (strncmp(reply, "RESETBE6F\r", 8) != 0)
    {
      ndiSetError(api, NDI_RESET_FAIL);
      return commandReply;
    }

    // terminate the reply string
    reply[bytes] = '\0';
    bytes -= 5;
    strncpy(commandReply, reply, bytes);
    commandReply[bytes] = '\0';

    // return the reply string, minus the CRC
    return commandReply;
  }

  vsprintf(command, format, ap);                    // format parameters

  unsigned short CRC16 = 0;                         // calculate CRC
  for (i = 0; command[i] != '\0'; i++)
  {
    CalcCRC16(command[i], &CRC16);
    if (inCommand && command[i] == ':')             // only use CRC if a ':'
    {
      useCrc = true;                                //  follows the command
    }
    if (inCommand && !((command[i] >= 'A' && command[i] <= 'Z') ||
                       (command[i] >= '0' && command[i] <= '9')))
    {
      inCommand = false;                            // 'command' part has ended
      commandLength = i;                            // command length
    }
  }
  if (inCommand)
  {
    // Command was sent with no ':'
    // Example, ndiCommand("INIT");
    commandLength = i;
  }

  if (useCrc)
  {
    sprintf(&command[i], "%04X", CRC16);            // tack on the CRC
    i += 4;
  }

  command[i++] = '\r';                              // tack on carriage return
  command[i] = '\0';                                // terminate for good luck

  bool isBinary = (strncmp(command, "BX", commandLength) == 0 && commandLength == strlen("BX") ||
                   strncmp(command, "GETLOG", commandLength) == 0 && commandLength == strlen("GETLOG") ||
                   strncmp(command, "VGET", commandLength) == 0 && commandLength == strlen("VGET"));


  // if the command is GX, TX, or BX and thread_mode is on, we copy the reply from
  //  the thread rather than getting it directly from the Measurement System
  if (api->IsThreadedMode && api->IsTracking &&
      commandLength == 2 && (command[0] == 'G' && command[1] == 'X' ||
                             command[0] == 'T' && command[1] == 'X' ||
                             command[0] == 'B' && command[1] == 'X'))
  {
    int errcode = 0;

    // check that the thread is sending the GX/BX/TX command that we want
    if (strcmp(command, api->ThreadCommand) != 0)
    {
      // tell thread to start using the new GX/BX/TX command
      ndiMutexLock(api->ThreadMutex);
      strcpy(api->ThreadCommand, command);
      api->IsThreadedCommandBinary = (command[0] == 'B');
      ndiMutexUnlock(api->ThreadMutex);
      // wait for the next data record to arrive (we have to throw it away)
      if (ndiEventWait(api->ThreadBufferEvent, 5000))
      {
        ndiSetError(api, NDI_TIMEOUT);
        return commandReply;
      }
    }
    // there is usually no wait, because usually new data is ready
    if (ndiEventWait(api->ThreadBufferEvent, 5000))
    {
      ndiSetError(api, NDI_TIMEOUT);
      return commandReply;
    }
    // copy the thread's reply buffer into the main reply buffer
    ndiMutexLock(api->ThreadBufferMutex);
    for (bytes = 0; api->ThreadBuffer[bytes] != '\0'; bytes++)
    {
      reply[bytes] = api->ThreadBuffer[bytes];
    }
    if (!isBinary)
    {
      reply[bytes] = '\0';   // terminate string
    }
    errcode = api->ThreadErrorCode;
    ndiMutexUnlock(api->ThreadBufferMutex);

    if (errcode != 0)
    {
      ndiSetError(api, errcode);
      return commandReply;
    }
  }
  // if the command is not a GX or thread_mode is not on, then
  //   send the command directly to the Measurement System and get a reply
  else
  {
    int errcode = 0;
    bool isThreadMode = api->IsThreadedMode;

    if (isThreadMode && api->IsTracking)
    {
      // block the tracking thread while we slip this command through
      ndiMutexLock(api->ThreadMutex);
    }

    // change pol->tracking if either TSTOP or TSTART is sent
    if ((commandLength == 5 && strncmp(command, "TSTOP", commandLength) == 0) ||
        (commandLength == 4 && strncmp(command, "INIT", commandLength) == 0))
    {
      api->IsTracking = false;
    }
    else if (commandLength == 6 && strncmp(command, "TSTART", commandLength) == 0)
    {
      api->IsTracking = true;
      if (isThreadMode)
      {
        // this will force the thread to wait until the application sends the first GX command
        api->ThreadCommand[0] = '\0';
      }
    }

    if (api->SerialDevice != NDI_INVALID_HANDLE)
    {
      // flush the input buffer, because anything that we haven't read
      //   yet is garbage left over by a previously failed command
      ndiSerialFlush(api->SerialDevice, NDI_IFLUSH);
    }
    else
    {
      ndiSocketFlush(api->Socket, NDI_IFLUSH);
    }

    // send the command to the Measurement System
    if (errcode == 0)
    {
      if (api->SerialDevice != NDI_INVALID_HANDLE)
      {
        bytes = ndiSerialWrite(api->SerialDevice, command, i);
      }
      else
      {
        bytes = ndiSocketWrite(api->Socket, command, i);
      }
      if (bytes < 0)
      {
        errcode = NDI_WRITE_ERROR;
      }
      else if (bytes < i)
      {
        errcode = NDI_TIMEOUT;
      }
    }

    // read the reply from the Measurement System
    bytes = 0;
    if (errcode == 0)
    {
      if (api->SerialDevice != NDI_INVALID_HANDLE)
      {
        bytes = ndiSerialRead(api->SerialDevice, reply, 2047, isBinary);
      }
      else
      {
        int errorCode;
        bytes = ndiSocketRead(api->Socket, reply, 2047, isBinary, &errorCode);
      }
      if (bytes < 0)
      {
        errcode = NDI_READ_ERROR;
        bytes = 0;
      }
      else if (bytes == 0)
      {
        errcode = NDI_TIMEOUT;
      }
      if (!isBinary)
      {
        reply[bytes] = '\0';   // terminate string
      }
    }

    if (isThreadMode & api->IsTracking)
    {
      // unblock the tracking thread
      ndiMutexUnlock(api->ThreadMutex);
    }

    if (errcode != 0)
    {
      ndiSetError(api, errcode);
      return commandReply;
    }
  }

  // back up to before the CRC
  if (!isBinary)
  {
    bytes -= 5; // 4 ASCII chars
  }
  else
  {
    bytes -= 2; // 2 bytes (unsigned short)
  }
  if (bytes < 0)
  {
    ndiSetError(api, NDI_BAD_CRC);
    return commandReply;
  }

  // calculate the CRC and copy serial_reply to command_reply
  CRC16 = 0;
  for (i = 0; i < bytes; i++)
  {
    CalcCRC16(reply[i], &CRC16);
    commandReply[i] = reply[i];
  }

  if (!isBinary)
  {
    // terminate command_reply before the CRC
    commandReply[i] = '\0';
  }

  if (!isBinary)
  {
    // read and check the CRC value of the reply
    if (CRC16 != ndiHexToUnsignedLong(&reply[bytes], 4))
    {
      ndiSetError(api, NDI_BAD_CRC);
      return commandReply;
    }
  }
  else
  {
    unsigned short replyCrc = (unsigned char)reply[bytes + 1] << 8 | (unsigned char)reply[bytes];
    if (replyCrc != CRC16)
    {
      ndiSetError(api, NDI_BAD_CRC);
      return commandReply;
    }
  }

  // check for error code
  if (commandReply[0] == 'E' && strncmp(commandReply, "ERROR", 5) == 0)
  {
    ndiSetError(api, ndiHexToUnsignedLong(&commandReply[5], 2));
    return commandReply;
  }

  // special behavior for specific commands
  if (command[0] == 'T' && command[1] == 'X' && commandLength == 2)   // the TX command
  {
    ndiTXHelper(api, command, commandReply);
  }
  else if (command[0] == 'B' && command[1] == 'X' && commandLength == 2)   // the BX command
  {
    ndiBXHelper(api, command, commandReply);
  }
  else if (command[0] == 'G' && command[1] == 'X' && commandLength == 2)   // the GX command
  {
    ndiGXHelper(api, command, commandReply);
  }
  else if (command[0] == 'C' && commandLength == 4 && strncmp(command, "COMM", commandLength) == 0)
  {
    ndiCOMMHelper(api, command, commandReply);
  }
  else if (command[0] == 'I' && commandLength == 4 && strncmp(command, "INIT", commandLength) == 0)
  {
    ndiINITHelper(api, command, commandReply);
  }
  else if (command[0] == 'I' && commandLength == 5 && strncmp(command, "IRCHK", commandLength) == 0)
  {
    ndiIRCHKHelper(api, command, commandReply);
  }
  else if (command[0] == 'P' && commandLength == 5 && strncmp(command, "PHINF", commandLength) == 0)
  {
    ndiPHINFHelper(api, command, commandReply);
  }
  else if (command[0] == 'P' && commandLength == 4 && strncmp(command, "PHRQ", commandLength) == 0)
  {
    ndiPHRQHelper(api, command, commandReply);
  }
  else if (command[0] == 'P' && commandLength == 4 && strncmp(command, "PHSR", commandLength) == 0)
  {
    ndiPHSRHelper(api, command, commandReply);
  }
  else if (command[0] == 'P' && commandLength == 5 && strncmp(command, "PSTAT", commandLength) == 0)
  {
    ndiPSTATHelper(api, command, commandReply);
  }
  else if (command[0] == 'S' && commandLength == 5 && strncmp(command, "SSTAT", commandLength) == 0)
  {
    ndiSSTATHelper(api, command, commandReply);
  }

  // return the Measurement System reply, but with the CRC hacked off
  return commandReply;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFPortStatus(ndicapi* pol)
{
  char* dp;

  dp = &pol->PhinfBasic[31];

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFToolInfo(ndicapi* pol, char information[31])
{
  char* dp;
  int i;

  dp = pol->PhinfBasic;

  for (i = 0; i < 31; i++)
  {
    information[i] = *dp++;
  }

  return pol->PhinfUnoccupied;
}

//----------------------------------------------------------------------------
ndicapiExport unsigned long ndiGetPHINFCurrentTest(ndicapi* pol)
{
  char* dp;

  dp = pol->PhinfTesting;

  return (int)ndiHexToUnsignedLong(dp, 8);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFPartNumber(ndicapi* pol, char part[20])
{
  char* dp;
  int i;

  dp = pol->PhinfPartNumber;

  for (i = 0; i < 20; i++)
  {
    part[i] = *dp++;
  }

  return pol->PhinfUnoccupied;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFAccessories(ndicapi* pol)
{
  char* dp;

  dp = pol->PhinfAccessories;

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFMarkerType(ndicapi* pol)
{
  char* dp;

  dp = pol->PhinfMarkerType;

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFPortLocation(ndicapi* pol, char location[14])
{
  char* dp;
  int i;

  dp = pol->PhinfPortLocation;

  for (i = 0; i < 14; i++)
  {
    location[i] = *dp++;
  }

  return pol->PhinfUnoccupied;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHINFGPIOStatus(ndicapi* pol)
{
  char* dp;

  dp = pol->PhinfGpioStatus;

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHRQHandle(ndicapi* pol)
{
  char* dp;

  dp = pol->PhrqReply;
  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHSRNumberOfHandles(ndicapi* pol)
{
  char* dp;

  dp = pol->PhsrReply;

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHSRHandle(ndicapi* pol, int i)
{
  char* dp;
  int n;

  dp = pol->PhsrReply;
  n = (int)ndiHexToUnsignedLong(dp, 2);
  dp += 2;

  if (i < 0 || i > n)
  {
    return 0;
  }
  dp += 5 * i;

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPHSRInformation(ndicapi* pol, int i)
{
  char* dp;
  int n;

  dp = pol->PhsrReply;
  n = (int)ndiHexToUnsignedLong(dp, 2);
  dp += 2;

  if (i < 0 || i > n)
  {
    return 0;
  }
  dp += 5 * i + 2;

  return (int)ndiHexToUnsignedLong(dp, 3);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXTransformf(ndicapi* pol, int portHandle, float transform[8])
{
  char* readPointer;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  readPointer = pol->TxTransforms[i];
  if (*readPointer == 'D' || *readPointer == '\0')
  {
    return NDI_DISABLED;
  }
  else if (*readPointer == 'M')
  {
    return NDI_MISSING;
  }

  transform[0] = ndiSignedToLong(&readPointer[0],  6) * 0.0001f;
  transform[1] = ndiSignedToLong(&readPointer[6],  6) * 0.0001f;
  transform[2] = ndiSignedToLong(&readPointer[12], 6) * 0.0001f;
  transform[3] = ndiSignedToLong(&readPointer[18], 6) * 0.0001f;
  transform[4] = ndiSignedToLong(&readPointer[24], 7) * 0.01f;
  transform[5] = ndiSignedToLong(&readPointer[31], 7) * 0.01f;
  transform[6] = ndiSignedToLong(&readPointer[38], 7) * 0.01f;
  transform[7] = ndiSignedToLong(&readPointer[45], 6) * 0.0001f;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXTransform(ndicapi* pol, int portHandle, double transform[8])
{
  char* readPointer;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  readPointer = pol->TxTransforms[i];
  if (*readPointer == 'D' || *readPointer == '\0')
  {
    return NDI_DISABLED;
  }
  else if (*readPointer == 'M')
  {
    return NDI_MISSING;
  }

  transform[0] = ndiSignedToLong(&readPointer[0], 6) * 0.0001;
  transform[1] = ndiSignedToLong(&readPointer[6], 6) * 0.0001;
  transform[2] = ndiSignedToLong(&readPointer[12], 6) * 0.0001;
  transform[3] = ndiSignedToLong(&readPointer[18], 6) * 0.0001;
  transform[4] = ndiSignedToLong(&readPointer[24], 7) * 0.01;
  transform[5] = ndiSignedToLong(&readPointer[31], 7) * 0.01;
  transform[6] = ndiSignedToLong(&readPointer[38], 7) * 0.01;
  transform[7] = ndiSignedToLong(&readPointer[45], 6) * 0.0001;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXPortStatus(ndicapi* pol, int ph)
{
  char* dp;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == ph)
    {
      break;
    }
  }
  if (i == n)
  {
    return 0;
  }

  dp = pol->TxStatus[i];

  return (int)ndiHexToUnsignedLong(dp, 8);
}

//----------------------------------------------------------------------------
ndicapiExport unsigned long ndiGetTXFrame(ndicapi* pol, int ph)
{
  char* dp;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == ph)
    {
      break;
    }
  }
  if (i == n)
  {
    return 0;
  }

  dp = pol->TxFrame[i];

  return (unsigned long)ndiHexToUnsignedLong(dp, 8);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXToolInfo(ndicapi* pol, int ph)
{
  char* dp;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == ph)
    {
      break;
    }
  }
  if (i == n)
  {
    return 0;
  }

  dp = pol->TxInformation[i];
  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXMarkerInfo(ndicapi* pol, int ph, int marker)
{
  char* dp;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == ph)
    {
      break;
    }
  }
  if (i == n || marker < 0 || marker >= 20)
  {
    return NDI_DISABLED;
  }

  dp = pol->TxInformation[2 + marker];
  return (int)ndiHexToUnsignedLong(dp, 1);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXSingleStray(ndicapi* pol, int ph, double coord[3])
{
  char* dp;
  int i, n;

  n = pol->TxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->TxHandles[i] == ph)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  dp = pol->TxSingleStray[i];
  if (*dp == 'D' || *dp == '\0')
  {
    return NDI_DISABLED;
  }
  else if (*dp == 'M')
  {
    return NDI_MISSING;
  }

  coord[0] = ndiSignedToLong(&dp[0],  7) * 0.01;
  coord[1] = ndiSignedToLong(&dp[7],  7) * 0.01;
  coord[2] = ndiSignedToLong(&dp[14], 7) * 0.01;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXNumberOfPassiveStrays(ndicapi* pol)
{
  return pol->TxPassiveStrayCount;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXPassiveStray(ndicapi* pol, int i, double coord[3])
{
  const char* dp;
  int n;

  dp = pol->TxPassiveStray;

  if (*dp == '\0')
  {
    return NDI_DISABLED;
  }

  n = pol->TxPassiveStrayCount;
  dp += 3;
  if (n < 0)
  {
    return NDI_MISSING;
  }
  if (n > 20)
  {
    n = 20;
  }

  if (i < 0 || i >= n)
  {
    return NDI_MISSING;
  }

  dp += 7 * 3 * i;
  coord[0] = ndiSignedToLong(&dp[0],  7) * 0.01;
  coord[1] = ndiSignedToLong(&dp[7],  7) * 0.01;
  coord[2] = ndiSignedToLong(&dp[14], 7) * 0.01;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetTXSystemStatus(ndicapi* pol)
{
  char* dp;

  dp = pol->TxSystemStatus;

  return (int)ndiHexToUnsignedLong(dp, 4);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXTransform(ndicapi* pol, int port, double transform[8])
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->GxTransforms[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->GxPassiveTransforms[port - 'A'];
  }
  else
  {
    return NDI_DISABLED;
  }

  if (*dp == 'D' || *dp == '\0')
  {
    return NDI_DISABLED;
  }
  else if (*dp == 'M')
  {
    return NDI_MISSING;
  }

  transform[0] = ndiSignedToLong(&dp[0],  6) * 0.0001;
  transform[1] = ndiSignedToLong(&dp[6],  6) * 0.0001;
  transform[2] = ndiSignedToLong(&dp[12], 6) * 0.0001;
  transform[3] = ndiSignedToLong(&dp[18], 6) * 0.0001;
  transform[4] = ndiSignedToLong(&dp[24], 7) * 0.01;
  transform[5] = ndiSignedToLong(&dp[31], 7) * 0.01;
  transform[6] = ndiSignedToLong(&dp[38], 7) * 0.01;
  transform[7] = ndiSignedToLong(&dp[45], 6) * 0.0001;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXPortStatus(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = &pol->GxStatus[6 - 2 * (port - '1')];
  }
  else if (port >= 'A' && port <= 'C')
  {
    dp = &pol->GxPassiveStatus[6 - 2 * (port - 'A')];
  }
  else if (port >= 'D' && port <= 'F')
  {
    dp = &pol->GxPassiveStatus[14 - 2 * (port - 'D')];
  }
  else if (port >= 'G' && port <= 'I')
  {
    dp = &pol->GxPassiveStatus[22 - 2 * (port - 'G')];
  }
  else
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXSystemStatus(ndicapi* pol)
{
  char* dp;

  dp = pol->GxStatus;

  if (*dp == '\0')
  {
    dp = pol->GxPassiveStatus;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXToolInfo(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->GxInformation[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->GxPassiveInformation[port - 'A'];
  }
  else
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXMarkerInfo(ndicapi* pol, int port, int marker)
{
  char* dp;

  if (marker < 'A' || marker > 'T')
  {
    return 0;
  }

  if (port >= '1' && port <= '3')
  {
    dp = pol->GxInformation[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->GxPassiveInformation[port - 'A'];
  }
  else
  {
    return 0;
  }
  dp += 11 - (marker - 'A');

  return (int)ndiHexToUnsignedLong(dp, 1);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXSingleStray(ndicapi* pol, int port, double coord[3])
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->GxSingleStray[port - '1'];
  }
  else
  {
    return NDI_DISABLED;
  }

  if (*dp == 'D' || *dp == '\0')
  {
    return NDI_DISABLED;
  }
  else if (*dp == 'M')
  {
    return NDI_MISSING;
  }

  coord[0] = ndiSignedToLong(&dp[0],  7) * 0.01;
  coord[1] = ndiSignedToLong(&dp[7],  7) * 0.01;
  coord[2] = ndiSignedToLong(&dp[14], 7) * 0.01;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport unsigned long ndiGetGXFrame(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->GxFrame[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->GxPassiveFrame[port - 'A'];
  }
  else
  {
    return 0;
  }

  return (unsigned long)ndiHexToUnsignedLong(dp, 8);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXNumberOfPassiveStrays(ndicapi* pol)
{
  const char* dp;
  int n;

  dp = pol->GxPassiveStray;

  if (*dp == '\0')
  {
    return 0;
  }

  n = (int)ndiSignedToLong(dp, 3);
  if (n < 0)
  {
    return 0;
  }
  if (n > 20)
  {
    return 20;
  }

  return n;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetGXPassiveStray(ndicapi* pol, int i, double coord[3])
{
  const char* dp;
  int n;

  dp = pol->GxPassiveStray;

  if (*dp == '\0')
  {
    return NDI_DISABLED;
  }

  n = (int)ndiSignedToLong(dp, 3);
  dp += 3;
  if (n < 0)
  {
    return NDI_MISSING;
  }
  if (n > 20)
  {
    n = 20;
  }

  if (i < 0 || i >= n)
  {
    return NDI_MISSING;
  }

  dp += 7 * 3 * i;
  coord[0] = ndiSignedToLong(&dp[0],  7) * 0.01;
  coord[1] = ndiSignedToLong(&dp[7],  7) * 0.01;
  coord[2] = ndiSignedToLong(&dp[14], 7) * 0.01;

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXTransform(ndicapi* pol, int portHandle, float transform[8])
{
  int i, n;

  n = pol->BxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->BxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  memcpy(&transform[0], &pol->BxTransforms[i][0], sizeof(float) * 8);
  if (pol->BxHandlesStatus[i] & NDI_HANDLE_DISABLED)
  {
    return NDI_DISABLED;
  }
  else if (pol->BxHandlesStatus[i] & NDI_HANDLE_MISSING)
  {
    return NDI_MISSING;
  }

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXPortStatus(ndicapi* pol, int portHandle)
{
  int i, n;

  n = pol->BxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->BxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return 0;
  }

  return pol->BxPortStatus[i];
}

//----------------------------------------------------------------------------
ndicapiExport unsigned long ndiGetBXFrame(ndicapi* pol, int portHandle)
{
  int i, n;

  n = pol->BxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->BxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return 0;
  }

  return pol->BxFrameNumber[i];
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXToolInfo(ndicapi* pol, int portHandle, char& outToolInfo)
{
  int i, n;

  n = pol->BxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->BxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  outToolInfo = pol->BxToolMarkerInformation[i][0];
  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXMarkerInfo(ndicapi* pol, int portHandle, int marker, char& outMarkerInfo)
{
  int i, n;

  if (marker >= 20)
  {
    return false;
  }

  n = pol->BxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->BxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  int byteIndex = marker / 2;
  if (marker % 2 == 0)
  {
    // low 4 bits, little endian format, so index backwards from the rear
    outMarkerInfo = pol->BxToolMarkerInformation[i][1 + (10 - byteIndex)] & 0x00FF;
  }
  else
  {
    // high 4 bits, little endian format, so index backwards from the rear
    outMarkerInfo = pol->BxToolMarkerInformation[i][1 + (10 - byteIndex)] & 0xFF00 >> 4;
  }
  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXSingleStray(ndicapi* pol, int portHandle, float outCoord[3])
{
  int i, n;

  n = pol->BxHandleCount;
  for (i = 0; i < n; i++)
  {
    if (pol->BxHandles[i] == portHandle)
    {
      break;
    }
  }
  if (i == n)
  {
    return NDI_DISABLED;
  }

  memcpy(outCoord, pol->BxActiveSingleStrayMarkerPosition[i], sizeof(float) * 3);
  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXNumberOfPassiveStrays(ndicapi* pol)
{
  return pol->BxPassiveStrayCount;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXPassiveStray(ndicapi* pol, int i, float outCoord[3])
{
  if (i > pol->BxPassiveStrayCount)
  {
    return NDI_DISABLED;
  }
  memcpy(outCoord, pol->BxPassiveStrayPosition[i], sizeof(float) * 3);
  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetBXSystemStatus(ndicapi* pol)
{
  return pol->BxSystemStatus;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPSTATPortStatus(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->PstatBasic[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->PstatPassiveBasic[port - 'A'];
  }
  else
  {
    return 0;
  }

  // the 'U' is for UNOCCUPIED
  if (*dp == 'U' || *dp == '\0')
  {
    return 0;
  }

  // skip to the last two characters
  dp += 30;

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPSTATToolInfo(ndicapi* pol, int port, char information[30])
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->PstatBasic[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->PstatPassiveBasic[port - 'A'];
  }
  else
  {
    return NDI_UNOCCUPIED;
  }

  // the 'U' is for UNOCCUPIED
  if (*dp == 'U' || *dp == '\0')
  {
    return NDI_UNOCCUPIED;
  }

  strncpy(information, dp, 30);

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport unsigned long ndiGetPSTATCurrentTest(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->PstatTesting[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->PstatPassiveTesting[port - 'A'];
  }
  else
  {
    return 0;
  }

  if (*dp == '\0')
  {
    return 0;
  }

  return (unsigned long)ndiHexToUnsignedLong(dp, 8);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPSTATPartNumber(ndicapi* pol, int port, char part[20])
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->PstatPartNumber[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->PstatPassivePartNumber[port - 'A'];
  }
  else
  {
    return NDI_UNOCCUPIED;
  }

  if (*dp == '\0')
  {
    return NDI_UNOCCUPIED;
  }

  strncpy(part, dp, 20);

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPSTATAccessories(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->PstatAccessories[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->PstatPassiveAccessories[port - 'A'];
  }
  else
  {
    return 0;
  }

  if (*dp == '\0')
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetPSTATMarkerType(ndicapi* pol, int port)
{
  char* dp;

  if (port >= '1' && port <= '3')
  {
    dp = pol->PstatMarkerType[port - '1'];
  }
  else if (port >= 'A' && port <= 'I')
  {
    dp = pol->PstatPassiveMarkerType[port - 'A'];
  }
  else
  {
    return 0;
  }

  if (*dp == '\0')
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetSSTATControl(ndicapi* pol)
{
  char* dp;

  dp = pol->SstatControl;

  if (*dp == '\0')
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetSSTATSensors(ndicapi* pol)
{
  char* dp;

  dp = pol->SstatSensor;

  if (*dp == '\0')
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetSSTATTIU(ndicapi* pol)
{
  char* dp;

  dp = pol->SstatTiu;

  if (*dp == '\0')
  {
    return 0;
  }

  return (int)ndiHexToUnsignedLong(dp, 2);
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetIRCHKDetected(ndicapi* pol)
{
  if (pol->IrchkDetected == '1')
  {
    return 1;
  }
  return 0;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetIRCHKNumberOfSources(ndicapi* pol, int side)
{
  const char* dp;
  int n, m;

  dp = pol->IrchkSources;

  if (*dp == '\0')
  {
    return 0;
  }

  n = (int)ndiSignedToLong(dp, 3);
  if (n < 0 || n > 20)
  {
    return 0;
  }
  m = (int)ndiSignedToLong(dp + 3 + 2 * 3 * n, 3);
  if (m < 0 || m > 20)
  {
    return 0;
  }

  if (side == NDI_LEFT)
  {
    return n;
  }
  else if (side == NDI_RIGHT)
  {
    return m;
  }

  return 0;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetIRCHKSourceXY(ndicapi* pol, int side, int i, double xy[2])
{
  const char* dp;
  int n, m;

  dp = pol->IrchkSources;

  if (dp == NULL || *dp == '\0')
  {
    return NDI_MISSING;
  }

  n = (int)ndiSignedToLong(dp, 3);
  if (n < 0 || n > 20)
  {
    return NDI_MISSING;
  }
  m = (int)ndiSignedToLong(dp + 3 + 2 * 3 * n, 3);
  if (m < 0 || m > 20)
  {
    return NDI_MISSING;
  }

  if (side == NDI_LEFT)
  {
    if (i < 0 || i >= n)
    {
      return NDI_MISSING;
    }
    dp += 3 + 2 * 3 * i;
  }
  else if (side == NDI_RIGHT)
  {
    if (i < 0 || i >= m)
    {
      return NDI_MISSING;
    }
    dp += 3 + 2 * 3 * n + 3 + 2 * 3 * i;
  }
  else if (side == NDI_RIGHT)
  {
    return NDI_MISSING;
  }

  xy[0] = ndiSignedToLong(&dp[0], 3) * 0.01;
  xy[1] = ndiSignedToLong(&dp[3], 3) * 0.01;

  return NDI_OKAY;
}


//----------------------------------------------------------------------------
// The tracking thread.
//
// This thread continually sends the most recent GX command to the
// NDICAPI until it is told to quit or until an error occurs.
//
// The thread is blocked unless the Measurement System is in tracking mode.
static void* ndiThreadFunc(void* userdata)
{
  int i, m;
  int errorCode = 0;
  char* command, *reply;
  ndicapi* pol;

  pol = (ndicapi*)userdata;
  command = pol->ThreadCommand;
  reply = pol->ThreadReply;

  while (errorCode == 0)
  {
    // if the application is blocking us, we sit here and wait
    ndiMutexLock(pol->ThreadMutex);

    // quit if threading has been turned off
    if (!pol->IsThreadedMode)
    {
      ndiMutexUnlock(pol->ThreadMutex);
      return NULL;
    }

    // check whether we have a GX/BX/TX command ready to send
    if (command[0] == '\0')
    {
      if (pol->SerialDevice != NDI_INVALID_HANDLE)
      {
        ndiSerialSleep(pol->SerialDevice, 20);
      }
      else
      {
        ndiSocketSleep(pol->Socket, 20);
      }
      ndiMutexUnlock(pol->ThreadMutex);
      continue;
    }

    // flush the input buffer, because anything that we haven't read
    //   yet is garbage left over by a previously failed command
    if (pol->SerialDevice != NDI_INVALID_HANDLE)
    {
      ndiSerialFlush(pol->SerialDevice, NDI_IFLUSH);
    }
    else
    {
      ndiSocketFlush(pol->Socket, NDI_IFLUSH);
    }

    // send the command to the Measurement System
    i = strlen(command);
    if (errorCode == 0)
    {
      if (pol->SerialDevice != NDI_INVALID_HANDLE)
      {
        m = ndiSerialWrite(pol->SerialDevice, command, i);
      }
      else
      {
        m = ndiSocketWrite(pol->Socket, command, i);
      }
      if (m < 0)
      {
        errorCode = NDI_WRITE_ERROR;
      }
      else if (m < i)
      {
        errorCode = NDI_TIMEOUT;
      }
    }

    // read the reply from the Measurement System
    if (errorCode == 0)
    {
      if (pol->SerialDevice != NDI_INVALID_HANDLE)
      {
        m = ndiSerialRead(pol->SerialDevice, reply, 2047, pol->IsThreadedCommandBinary);
      }
      else
      {
        int errorCode;
        m = ndiSocketRead(pol->Socket, reply, 2047, pol->IsThreadedCommandBinary, &errorCode);
      }
      if (m < 0)
      {
        errorCode = NDI_READ_ERROR;
        m = 0;
      }
      else if (m == 0)
      {
        errorCode = NDI_TIMEOUT;
      }
      // terminate the string
      reply[m] = '\0';
    }

    // lock the buffer
    ndiMutexLock(pol->ThreadBufferMutex);
    // copy the reply into the buffer, also copy the error code
    strcpy(pol->ThreadBuffer, reply);
    pol->ThreadErrorCode = errorCode;
    // signal the main thread that a new data record is ready
    ndiEventSignal(pol->ThreadBufferEvent);
    // unlock the buffer
    ndiMutexUnlock(pol->ThreadBufferMutex);

    // release the lock to give the application a chance to block us
    ndiMutexUnlock(pol->ThreadMutex);
  }

  return NULL;
}

//----------------------------------------------------------------------------
// Allocate all the objects needed for threading and then start the thread.
static void ndiSpawnThread(ndicapi* pol)
{
  pol->ThreadCommand = (char*)malloc(2048);
  pol->ThreadCommand[0] = '\0';
  pol->ThreadReply = (char*)malloc(2048);
  pol->ThreadReply[0] = '\0';
  pol->ThreadBuffer = (char*)malloc(2048);
  pol->ThreadBuffer[0] = '\0';
  pol->ThreadErrorCode = 0;

  pol->ThreadBufferMutex = ndiMutexCreate();
  pol->ThreadBufferEvent = ndiEventCreate();
  pol->ThreadMutex = ndiMutexCreate();
  if (!pol->IsTracking)
  {
    // if not tracking, then block the thread
    ndiMutexLock(pol->ThreadMutex);
  }
  pol->Thread = ndiThreadSplit(&ndiThreadFunc, pol);
}

//----------------------------------------------------------------------------
// Wait for the tracking thread to end, and then do the clean - up.
static void ndiJoinThread(ndicapi* pol)
{
  if (!pol->IsTracking)
  {
    // if not tracking, unblock the thread or it can't stop
    ndiMutexUnlock(pol->ThreadMutex);
  }
  ndiThreadJoin(pol->Thread);
  ndiEventDestroy(pol->ThreadBufferEvent);
  ndiMutexDestroy(pol->ThreadBufferMutex);
  ndiMutexDestroy(pol->ThreadMutex);

  free(pol->ThreadBuffer);
  pol->ThreadBuffer = 0;
  free(pol->ThreadReply);
  pol->ThreadReply = 0;
  free(pol->ThreadCommand);
  pol->ThreadCommand = 0;
}

//----------------------------------------------------------------------------
// For starting and stopping the tracking thread.
//
// The mode is either 0(no thread) or 1(thread).  Other modes
// might be added in the future.
ndicapiExport void ndiSetThreadMode(ndicapi* pol, bool mode)
{
  if ((pol->IsThreadedMode && mode) || (!pol->IsThreadedMode && !mode))
  {
    return;
  }

  pol->IsThreadedMode = mode;

  if (mode)
  {
    ndiSpawnThread(pol);
  }
  else
  {
    ndiJoinThread(pol);
  }
}

//----------------------------------------------------------------------------
ndicapiExport int ndiGetThreadMode(ndicapi* pol)
{
  return pol->IsThreadedMode;
}