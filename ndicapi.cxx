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
#include "ndicapi_thread.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(__APPLE__)
  #include <dirent.h>
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
  char* SerialCommand;                    // text sent to the ndicapi
  char* SerialReply;                      // reply from the ndicapi

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
  int ThreadErrorCode;                    // error code to go with buffer

  // command reply -- this is the return value from plCommand()
  char* CommandReply;                     // reply without CRC and <CR>

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
  int TxNhandles;
  unsigned char TxHandles[NDI_MAX_HANDLES];
  char TxTransforms[NDI_MAX_HANDLES][52];
  char TxStatus[NDI_MAX_HANDLES][8];
  char TxFrame[NDI_MAX_HANDLES][8];
  char TxInformation[NDI_MAX_HANDLES][12];
  char TxSingleStray[NDI_MAX_HANDLES][24];
  char TxSystemStatus[4];

  int TxNpassiveStray;
  char TxPassiveStrayOov[14];
  char TxPassiveStray[1052];
};

//----------------------------------------------------------------------------
// Prototype for the error helper function, the definition is at the
// end of this file.  A call to this function will both set ndicapi
// error indicator, and will also call the error callback function if
// there is one.  The return value is equal to errnum.
namespace
{
  int ndi_set_error(ndicapi* pol, int errnum)
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
ndicapiExport unsigned long ndiHexToUnsignedLong(const char* cp, int n)
{
  int i;
  unsigned long result = 0;
  int c;

  for (i = 0; i < n; i++)
  {
    c = cp[i];
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
ndicapiExport char* ndiDeviceName(int i)
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

  // Linux/unix variants

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
ndicapiExport int ndiProbe(const char* device)
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
      ndiSerialRead(serial_port, init_reply, 16) <= 0 || strncmp(init_reply, "OKAYA896\r", 9) != 0)
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

    n = ndiSerialRead(serial_port, init_reply, 16);
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
    n = ndiSerialRead(serial_port, init_reply, 16);
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

  // use VER command to verify that this is a NDI device
  ndiSerialSleep(serial_port, 100);
  if (ndiSerialWrite(serial_port, "VER:065EE\r", 10) < 10 ||
      (n = ndiSerialRead(serial_port, reply, 1023)) < 7)
  {
    ndiSerialClose(serial_port);
    return NDI_PROBE_FAIL;
  }

  // restore things back to the way they were
  ndiSerialClose(serial_port);

  return NDI_OKAY;
}

//----------------------------------------------------------------------------
ndicapiExport ndicapi* ndiOpen(const char* device)
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
  pol->SerialCommand = (char*)malloc(2048);
  pol->SerialReply = (char*)malloc(2048);
  pol->CommandReply = (char*)malloc(2048);

  if (pol->SerialDeviceName == 0 || pol->SerialCommand == 0 || pol->SerialReply == 0 || pol->CommandReply == 0)
  {
    if (pol->SerialDeviceName)
    {
      free(pol->SerialDeviceName);
    }
    if (pol->SerialCommand)
    {
      free(pol->SerialCommand);
    }
    if (pol->SerialReply)
    {
      free(pol->SerialReply);
    }
    if (pol->CommandReply)
    {
      free(pol->CommandReply);
    }

    ndiSerialClose(serial_port);
    return NULL;
  }

  // initialize the allocated memory
  strcpy(pol->SerialDeviceName, device);
  memset(pol->SerialCommand, 0, 2048);
  memset(pol->SerialReply, 0, 2048);
  memset(pol->CommandReply, 0, 2048);

  return pol;
}

//----------------------------------------------------------------------------
ndicapiExport char* ndiGetDeviceName(ndicapi* pol)
{
  return pol->SerialDeviceName;
}

//----------------------------------------------------------------------------
ndicapiExport NDIFileHandle ndiGetDeviceHandle(ndicapi* pol)
{
  return pol->SerialDevice;
}

//----------------------------------------------------------------------------
ndicapiExport void ndiClose(ndicapi* pol)
{
  // end the tracking thread if it is running
  ndiSetThreadMode(pol, 0);

  // close the serial port
  ndiSerialClose(pol->SerialDevice);

  // free the buffers
  free(pol->SerialDeviceName);
  free(pol->SerialCommand);
  free(pol->SerialReply);
  free(pol->CommandReply);

  free(pol);
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
  void ndi_PHINF_helper(ndicapi* pol, const char* cp, const char* crp)
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
  void ndi_PHRQ_helper(ndicapi* pol, const char* cp, const char* crp)
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
  void ndi_PHSR_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    char* dp;
    int j;

    dp = pol->PhsrReply;
    for (j = 0; j < 1282 && *crp >= ' '; j++)
    {
      *dp++ = *crp++;
    }
    *dp++ = '\0';
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
  void ndi_TX_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    unsigned long mode = 0x0001; // the default reply mode
    char* dp;
    int i, j, n;
    int ph, nhandles, nstray;

    // if the TX command had a reply mode, read it
    if ((cp[2] == ':' && cp[7] != '\r') || (cp[2] == ' ' && cp[3] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&cp[3], 4);
    }

    // get the number of handles
    nhandles = (int)ndiHexToUnsignedLong(crp, 2);
    for (j = 0; j < 2 && *crp >= ' '; j++)
    {
      crp++;
    }

    // go through the information for each handle
    for (i = 0; i < nhandles; i++)
    {
      // get the handle itself (two chars)
      ph = (int)ndiHexToUnsignedLong(crp, 2);
      for (j = 0; j < 2 && *crp >= ' '; j++)
      {
        crp++;
      }

      // check for "UNOCCUPIED"
      if (*crp == 'U')
      {
        for (j = 0; j < 10 && *crp >= ' '; j++)
        {
          crp++;
        }
        // back up and continue (don't store information for unoccupied ports)
        i--;
        nhandles--;
        continue;
      }

      // save the port handle in the list
      pol->TxHandles[i] = ph;

      if (mode & NDI_XFORMS_AND_STATUS)
      {
        // get the transform, MISSING, or DISABLED
        dp = pol->TxTransforms[i];

        if (*crp == 'M')
        {
          // check for "MISSING"
          for (j = 0; j < 7 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
        }
        else if (*crp == 'D')
        {
          // check for "DISABLED"
          for (j = 0; j < 8 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
        }
        else
        {
          // read the transform
          for (j = 0; j < 51 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
        }
        *dp = '\0';

        // get the status
        dp = pol->TxStatus[i];
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }

        // get the frame number
        dp = pol->TxFrame[i];
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }

      // grab additonal information
      if (mode & NDI_ADDITIONAL_INFO)
      {
        dp = pol->TxInformation[i];
        for (j = 0; j < 20 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
      }

      // grab the single marker info
      if (mode & NDI_SINGLE_STRAY)
      {
        dp = pol->TxSingleStray[i];
        if (*crp == 'M')
        {
          // check for "MISSING"
          for (j = 0; j < 7 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
        }
        else if (*crp == 'D')
        {
          // check for "DISABLED"
          for (j = 0; j < 8 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
        }
        else
        {
          // read the single stray position
          for (j = 0; j < 21 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
        }
        *dp = '\0';
      }

      // skip over any unsupported information
      while (*crp >= ' ')
      {
        crp++;
      }

      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    // save the number of handles (minus the unoccupied handles)
    pol->TxNhandles = nhandles;

    // get all the passive stray information
    // this will be a maximum of 3 + 13 + 50*3*7 = 1066 bytes
    if (mode & NDI_PASSIVE_STRAY)
    {
      // get the number of strays
      nstray = (int)ndiSignedToLong(crp, 3);
      for (j = 0; j < 2 && *crp >= ' '; j++)
      {
        crp++;
      }
      if (nstray > 50)
      {
        nstray = 50;
      }
      pol->TxNpassiveStray = nstray;
      // get the out-of-volume bits
      dp = pol->TxPassiveStrayOov;
      n = (nstray + 3) / 4;
      for (j = 0; j < n && *crp >= ' '; j++)
      {
        *dp++ = *crp++;
      }
      // get the coordinates
      dp = pol->TxPassiveStray;
      n = nstray * 21;
      for (j = 0; j < n && *crp >= ' '; j++)
      {
        *dp++ = *crp++;
      }
      *dp = '\0';
    }

    // get the system status
    dp = pol->TxSystemStatus;
    for (j = 0; j < 4 && *crp >= ' '; j++)
    {
      *dp++ = *crp++;
    }
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
  void ndi_GX_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    unsigned long mode = 0x0001; // the default reply mode
    char* dp;
    int i, j, k;
    int npassive, nactive;

    // if the GX command had a reply mode, read it
    if ((cp[2] == ':' && cp[7] != '\r') || (cp[2] == ' ' && cp[3] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&cp[3], 4);
    }

    // always three active ports
    nactive = 3;

    if (mode & NDI_XFORMS_AND_STATUS)
    {
      for (k = 0; k < nactive; k += 3)
      {
        // grab the three transforms
        for (i = 0; i < 3; i++)
        {
          dp = pol->GxTransforms[i];
          for (j = 0; j < 51 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
          *dp = '\0';
          // fprintf(stderr, "xf %.51s\n", pol->gx_transforms[i]);
          // eat the trailing newline
          if (*crp == '\n')
          {
            crp++;
          }
        }
        // grab the status flags
        dp = pol->GxStatus + k / 3 * 8;
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "st %.8s\n", pol->gx_status);
      }
      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    if (mode & NDI_ADDITIONAL_INFO)
    {
      // grab information for each port
      for (i = 0; i < nactive; i++)
      {
        dp = pol->GxInformation[i];
        for (j = 0; j < 12 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "ai %.12s\n", pol->gx_information[i]);
      }
      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    if (mode & NDI_SINGLE_STRAY)
    {
      // grab stray marker for each port
      for (i = 0; i < nactive; i++)
      {
        dp = pol->GxSingleStray[i];
        for (j = 0; j < 21 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        *dp = '\0';
        // fprintf(stderr, "ss %.21s\n", pol->gx_single_stray[i]);
        // eat the trailing newline
        if (*crp == '\n')
        {
          crp++;
        }
      }
    }

    if (mode & NDI_FRAME_NUMBER)
    {
      // get frame number for each port
      for (i = 0; i < nactive; i++)
      {
        dp = pol->GxFrame[i];
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "fn %.8s\n", pol->gx_frame[i]);
      }
      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    // if there is no passive information, stop here
    if (!(mode & NDI_PASSIVE))
    {
      return;
    }

    // in case there are 9 passive tools instead of just 3
    npassive = 3;
    if (mode & NDI_PASSIVE_EXTRA)
    {
      npassive = 9;
    }

    if ((mode & NDI_XFORMS_AND_STATUS) || (mode == NDI_PASSIVE))
    {
      // the information is grouped in threes
      for (k = 0; k < npassive; k += 3)
      {
        // grab the three transforms
        for (i = 0; i < 3; i++)
        {
          dp = pol->GxPassiveTransforms[k + i];
          for (j = 0; j < 51 && *crp >= ' '; j++)
          {
            *dp++ = *crp++;
          }
          *dp = '\0';
          // fprintf(stderr, "pxf %.31s\n", pol->gx_passive_transforms[k+i]);
          // eat the trailing newline
          if (*crp == '\n')
          {
            crp++;
          }
        }
        // grab the status flags
        dp = pol->GxPassiveStatus + k / 3 * 8;
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pst %.8s\n", pol->gx_passive_status + k/3*8);
        // skip the newline
        if (*crp == '\n')
        {
          crp++;
        }
        else   // no newline: no more passive transforms
        {
          npassive = k + 3;
        }
      }
      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    if (mode & NDI_ADDITIONAL_INFO)
    {
      // grab information for each port
      for (i = 0; i < npassive; i++)
      {
        dp = pol->GxPassiveInformation[i];
        for (j = 0; j < 12 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pai %.12s\n", pol->gx_passive_information[i]);
      }
      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    if (mode & NDI_FRAME_NUMBER)
    {
      // get frame number for each port
      for (i = 0; i < npassive; i++)
      {
        dp = pol->GxPassiveFrame[i];
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pfn %.8s\n", pol->gx_passive_frame[i]);
      }
      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    if (mode & NDI_PASSIVE_STRAY)
    {
      // get all the passive stray information
      // this will be a maximum of 3 + 20*3*7 = 423 bytes
      dp = pol->GxPassiveStray;
      for (j = 0; j < 423 && *crp >= ' '; j++)
      {
        *dp++ = *crp++;
      }
      // fprintf(stderr, "psm %s\n", pol->gx_passive_stray);
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the PSTAT reply information into the ndicapi structure.
  void ndi_PSTAT_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    unsigned long mode = 0x0001; // the default reply mode
    char* dp;
    int i, j;
    int npassive, nactive;

    // if the PSTAT command had a reply mode, read it
    if ((cp[5] == ':' && cp[10] != '\r') || (cp[5] == ' ' && cp[6] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&cp[6], 4);
    }

    // always three active ports
    nactive = 3;

    // information for each port is separated by a newline
    for (i = 0; i < nactive; i++)
    {

      // basic tool information and port status
      if (mode & NDI_BASIC)
      {
        dp = pol->PstatBasic[i];
        for (j = 0; j < 32 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // terminate if UNOCCUPIED
        if (j < 32)
        {
          *dp = '\0';
        }
        // fprintf(stderr, "ba %.32s\n", pol->pstat_basic[i]);
      }

      // current testing
      if (mode & NDI_TESTING)
      {
        dp = pol->PstatTesting[i];
        *dp = '\0';
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "ai %.8s\n", pol->pstat_testing[i]);
      }

      // part number
      if (mode & NDI_PART_NUMBER)
      {
        dp = pol->PstatPartNumber[i];
        *dp = '\0';
        for (j = 0; j < 20 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pn %.20s\n", pol->pstat_part_number[i]);
      }

      // accessories
      if (mode & NDI_ACCESSORIES)
      {
        dp = pol->PstatAccessories[i];
        *dp = '\0';
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "ac %.2s\n", pol->pstat_accessories[i]);
      }

      // marker type
      if (mode & NDI_MARKER_TYPE)
      {
        dp = pol->PstatMarkerType[i];
        *dp = '\0';
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "mt %.2s\n", pol->pstat_marker_type[i]);
      }

      // skip any other information that might be present
      while (*crp >= ' ')
      {
        crp++;
      }

      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }

    // if there is no passive information, stop here
    if (!(mode & NDI_PASSIVE))
    {
      return;
    }

    // in case there are 9 passive tools instead of just 3
    npassive = 3;
    if (mode & NDI_PASSIVE_EXTRA)
    {
      npassive = 9;
    }

    // information for each port is separated by a newline
    for (i = 0; i < npassive; i++)
    {

      // basic tool information and port status
      if (mode & NDI_BASIC)
      {
        dp = pol->PstatPassiveBasic[i];
        *dp = '\0';
        for (j = 0; j < 32 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // terminate if UNOCCUPIED
        if (j < 32)
        {
          *dp = '\0';
        }
        // fprintf(stderr, "pba %.32s\n", pol->pstat_passive_basic[i]);
      }

      // current testing
      if (mode & NDI_TESTING)
      {
        dp = pol->PstatPassiveTesting[i];
        *dp = '\0';
        for (j = 0; j < 8 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pai %.8s\n", pol->pstat_passive_testing[i]);
      }

      // part number
      if (mode & NDI_PART_NUMBER)
      {
        dp = pol->PstatPassivePartNumber[i];
        *dp = '\0';
        for (j = 0; j < 20 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "ppn %.20s\n", pol->pstat_passive_part_number[i]);
      }

      // accessories
      if (mode & NDI_ACCESSORIES)
      {
        dp = pol->PstatPassiveAccessories[i];
        *dp = '\0';
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pac %.2s\n", pol->pstat_passive_accessories[i]);
      }

      // marker type
      if (mode & NDI_MARKER_TYPE)
      {
        dp = pol->PstatPassiveMarkerType[i];
        *dp = '\0';
        for (j = 0; j < 2 && *crp >= ' '; j++)
        {
          *dp++ = *crp++;
        }
        // fprintf(stderr, "pmt %.2s\n", pol->pstat_passive_marker_type[i]);
      }

      // skip any other information that might be present
      while (*crp >= ' ')
      {
        crp++;
      }

      // eat the trailing newline
      if (*crp == '\n')
      {
        crp++;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Copy all the SSTAT reply information into the ndicapi structure.
  void ndi_SSTAT_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    unsigned long mode;
    char* dp;

    // read the SSTAT command reply mode
    mode = ndiHexToUnsignedLong(&cp[6], 4);

    if (mode & NDI_CONTROL)
    {
      dp = pol->SstatControl;
      *dp++ = *crp++;
      *dp++ = *crp++;
    }

    if (mode & NDI_SENSORS)
    {
      dp = pol->SstatSensor;
      *dp++ = *crp++;
      *dp++ = *crp++;
    }

    if (mode & NDI_TIU)
    {
      dp = pol->SstatTiu;
      *dp++ = *crp++;
      *dp++ = *crp++;
    }

  }

  //----------------------------------------------------------------------------
  // Copy all the IRCHK reply information into the ndicapi structure.
  void ndi_IRCHK_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    unsigned long mode = 0x0001; // the default reply mode
    int j;

    // if the IRCHK command had a reply mode, read it
    if ((cp[5] == ':' && cp[10] != '\r') || (cp[5] == ' ' && cp[6] != '\r'))
    {
      mode = ndiHexToUnsignedLong(&cp[6], 4);
    }

    // a single character, '0' or '1'
    if (mode & NDI_DETECTED)
    {
      pol->IrchkDetected = *crp++;
    }

    // maximum string length for 20 sources is 2*(3 + 20*3) = 126
    // copy until a control char (less than 0x20) is found
    if (mode & NDI_SOURCES)
    {
      for (j = 0; j < 126 && *crp >= ' '; j++)
      {
        pol->IrchkSources[j] = *crp++;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Adjust the host to match a COMM command.
  void ndi_COMM_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    static int convert_baud[8] = { 9600, 14400, 19200, 38400, 57600, 115200, 921600, 1228739 };
    char newdps[4] = "8N1";
    int newspeed = 9600;
    int newhand = 0;

    if (cp[5] >= '0' && cp[5] <= '7')
    {
      newspeed = convert_baud[cp[5] - '0'];
    }
    if (cp[6] == '1')
    {
      newdps[0] = '7';
    }
    if (cp[7] == '1')
    {
      newdps[1] = 'O';
    }
    else if (cp[7] == '2')
    {
      newdps[1] = 'E';
    }
    if (cp[8] == '1')
    {
      newdps[2] = '2';
    }
    if (cp[9] == '1')
    {
      newhand = 1;
    }

    ndiSerialSleep(pol->SerialDevice, 100);  // let the device adjust itself
    if (ndiSerialComm(pol->SerialDevice, newspeed, newdps, newhand) != 0)
    {
      ndi_set_error(pol, NDI_BAD_COMM);
    }
  }

  //----------------------------------------------------------------------------
  // Sleep for 100 milliseconds after an INIT command.
  void ndi_INIT_helper(ndicapi* pol, const char* cp, const char* crp)
  {
    ndiSerialSleep(pol->SerialDevice, 100);
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
ndicapiExport char* ndiCommandVA(ndicapi* pol, const char* format, va_list ap)
{
  int i, m, nc;
  unsigned int CRC16 = 0;
  int use_crc = 0;
  int in_command = 1;
  char* cp, *rp, *crp;

  cp = pol->SerialCommand;      // text sent to ndicapi
  rp = pol->SerialReply;        // text received from ndicapi
  crp = pol->CommandReply;      // received text, with CRC hacked off
  nc = 0;                        // length of 'command' part of command

  pol->ErrorCode = 0;           // clear error
  cp[0] = '\0';
  rp[0] = '\0';
  crp[0] = '\0';

  // verify that the serial device was opened
  if (pol->SerialDevice == NDI_INVALID_HANDLE)
  {
    ndi_set_error(pol, NDI_OPEN_ERROR);
    return crp;
  }

  // if the command is NULL, send a break to reset the Measurement System
  if (format == NULL)
  {

    if (pol->IsThreadedMode && pol->IsTracking)
    {
      // block the tracking thread
      ndiMutexLock(pol->ThreadMutex);
    }
    pol->IsTracking = false;

    ndiSerialComm(pol->SerialDevice, 9600, "8N1", 0);
    ndiSerialFlush(pol->SerialDevice, NDI_IOFLUSH);
    ndiSerialBreak(pol->SerialDevice);
    m = ndiSerialRead(pol->SerialDevice, rp, 2047);

    // check for correct reply
    if (strncmp(rp, "RESETBE6F\r", 8) != 0)
    {
      ndi_set_error(pol, NDI_RESET_FAIL);
      return crp;
    }

    // terminate the reply string
    rp[m] = '\0';
    m -= 5;
    strncpy(crp, rp, m);
    crp[m] = '\0';

    // return the reply string, minus the CRC
    return crp;
  }

  vsprintf(cp, format, ap);                   // format parameters

  CRC16 = 0;                                  // calculate CRC
  for (i = 0; cp[i] != '\0'; i++)
  {
    CalcCRC16(cp[i], &CRC16);
    if (in_command && cp[i] == ':')           // only use CRC if a ':'
    {
      use_crc = 1;                            //  follows the command
    }
    if (in_command &&
        !((cp[i] >= 'A' && cp[i] <= 'Z') ||
          (cp[i] >= '0' && cp[i] <= '9')))
    {
      in_command = 0;                         // 'command' part has ended
      nc = i;                                 // command length
    }
  }

  if (use_crc)
  {
    sprintf(&cp[i], "%04X", CRC16);           // tack on the CRC
    i += 4;
  }

  cp[i] = '\0';

  cp[i++] = '\r';                             // tack on carriage return
  cp[i] = '\0';                               // terminate for good luck

  // if the command is GX and thread_mode is on, we copy the reply from
  //  the thread rather than getting it directly from the Measurement System
  if (pol->IsThreadedMode && pol->IsTracking &&
      nc == 2 && (cp[0] == 'G' && cp[1] == 'X' ||
                  cp[0] == 'T' && cp[1] == 'X' ||
                  cp[0] == 'B' && cp[1] == 'X'))
  {
    int errcode = 0;

    // check that the thread is sending the GX command that we want
    if (strcmp(cp, pol->ThreadCommand) != 0)
    {
      // tell thread to start using the new GX command
      ndiMutexLock(pol->ThreadMutex);
      strcpy(pol->ThreadCommand, cp);
      ndiMutexUnlock(pol->ThreadMutex);
      // wait for the next data record to arrive (we have to throw it away)
      if (ndiEventWait(pol->ThreadBufferEvent, 5000))
      {
        ndi_set_error(pol, NDI_TIMEOUT);
        return crp;
      }
    }
    // there is usually no wait, because usually new data is ready
    if (ndiEventWait(pol->ThreadBufferEvent, 5000))
    {
      ndi_set_error(pol, NDI_TIMEOUT);
      return crp;
    }
    // copy the thread's reply buffer into the main reply buffer
    ndiMutexLock(pol->ThreadBufferMutex);
    for (m = 0; pol->ThreadBuffer[m] != '\0'; m++)
    {
      rp[m] = pol->ThreadBuffer[m];
    }
    rp[m] = '\0';   // terminate string
    errcode = pol->ThreadErrorCode;
    ndiMutexUnlock(pol->ThreadBufferMutex);

    if (errcode != 0)
    {
      ndi_set_error(pol, errcode);
      return crp;
    }
  }
  // if the command is not a GX or thread_mode is not on, then
  //   send the command directly to the Measurement System and get a reply
  else
  {
    int errcode = 0;
    bool isThreadMode;

    // guard against pol->thread_mode changing while mutex is locked
    isThreadMode = pol->IsThreadedMode;

    if (isThreadMode && pol->IsTracking)
    {
      // block the tracking thread while we slip this command through
      ndiMutexLock(pol->ThreadMutex);
    }

    // change  pol->tracking  if either TSTOP or TSTART is sent
    if ((nc == 5 && strncmp(cp, "TSTOP", nc) == 0) ||
        (nc == 4 && strncmp(cp, "INIT", nc) == 0))
    {
      pol->IsTracking = false;
    }
    else if (nc == 6 && strncmp(cp, "TSTART", nc) == 0)
    {
      pol->IsTracking = true;
      if (isThreadMode)
      {
        // this will force the thread to wait until the application sends the first GX command
        pol->ThreadCommand[0] = '\0';
      }
    }

    // flush the input buffer, because anything that we haven't read
    //   yet is garbage left over by a previously failed command
    ndiSerialFlush(pol->SerialDevice, NDI_IFLUSH);

    // send the command to the Measurement System
    if (errcode == 0)
    {
      m = ndiSerialWrite(pol->SerialDevice, cp, i);
      if (m < 0)
      {
        errcode = NDI_WRITE_ERROR;
      }
      else if (m < i)
      {
        errcode = NDI_TIMEOUT;
      }
    }

    // read the reply from the Measurement System
    m = 0;
    if (errcode == 0)
    {
      m = ndiSerialRead(pol->SerialDevice, rp, 2047);
      if (m < 0)
      {
        errcode = NDI_WRITE_ERROR;
        m = 0;
      }
      else if (m == 0)
      {
        errcode = NDI_TIMEOUT;
      }
      rp[m] = '\0';   // terminate string
    }

    if (isThreadMode & pol->IsTracking)
    {
      // unblock the tracking thread
      // fprintf(stderr,"unlock\n");
      ndiMutexUnlock(pol->ThreadMutex);
      // fprintf(stderr,"unlocked\n");
    }

    if (errcode != 0)
    {
      ndi_set_error(pol, errcode);
      return crp;
    }
  }

  // back up to before the CRC
  m -= 5;
  if (m < 0)
  {
    ndi_set_error(pol, NDI_BAD_CRC);
    return crp;
  }

  // calculate the CRC and copy serial_reply to command_reply
  CRC16 = 0;
  for (i = 0; i < m; i++)
  {
    CalcCRC16(rp[i], &CRC16);
    crp[i] = rp[i];
  }

  // terminate command_reply before the CRC
  crp[i] = '\0';

  // read and check the CRC value of the reply
  if (CRC16 != ndiHexToUnsignedLong(&rp[m], 4))
  {
    ndi_set_error(pol, NDI_BAD_CRC);
    return crp;
  }

  // check for error code
  if (crp[0] == 'E' && strncmp(crp, "ERROR", 5) == 0)
  {
    ndi_set_error(pol, ndiHexToUnsignedLong(&crp[5], 2));
    return crp;
  }

  // special behavior for specific commands
  if (cp[0] == 'T' && cp[1] == 'X' && nc == 2)   // the TX command
  {
    ndi_TX_helper(pol, cp, crp);
  }
  else if (cp[0] == 'G' && cp[1] == 'X' && nc == 2)   // the GX command
  {
    ndi_GX_helper(pol, cp, crp);
  }
  else if (cp[0] == 'C' && nc == 4 && strncmp(cp, "COMM", nc) == 0)
  {
    ndi_COMM_helper(pol, cp, crp);
  }
  else if (cp[0] == 'I' && nc == 4 && strncmp(cp, "INIT", nc) == 0)
  {
    ndi_INIT_helper(pol, cp, crp);
  }
  else if (cp[0] == 'I' && nc == 5 && strncmp(cp, "IRCHK", nc) == 0)
  {
    ndi_IRCHK_helper(pol, cp, crp);
  }
  else if (cp[0] == 'P' && nc == 5 && strncmp(cp, "PHINF", nc) == 0)
  {
    ndi_PHINF_helper(pol, cp, crp);
  }
  else if (cp[0] == 'P' && nc == 4 && strncmp(cp, "PHRQ", nc) == 0)
  {
    ndi_PHRQ_helper(pol, cp, crp);
  }
  else if (cp[0] == 'P' && nc == 4 && strncmp(cp, "PHSR", nc) == 0)
  {
    ndi_PHSR_helper(pol, cp, crp);
  }
  else if (cp[0] == 'P' && nc == 5 && strncmp(cp, "PSTAT", nc) == 0)
  {
    ndi_PSTAT_helper(pol, cp, crp);
  }
  else if (cp[0] == 'S' && nc == 5 && strncmp(cp, "SSTAT", nc) == 0)
  {
    ndi_SSTAT_helper(pol, cp, crp);
  }

  // return the Measurement System reply, but with the CRC hacked off
  return crp;
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
ndicapiExport int ndiGetTXTransform(ndicapi* pol, int ph, double transform[8])
{
  char* dp;
  int i, n;

  n = pol->TxNhandles;
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

  dp = pol->TxTransforms[i];
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
ndicapiExport int ndiGetTXPortStatus(ndicapi* pol, int ph)
{
  char* dp;
  int i, n;

  n = pol->TxNhandles;
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

  n = pol->TxNhandles;
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

  n = pol->TxNhandles;
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

  n = pol->TxNhandles;
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

  n = pol->TxNhandles;
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
  return pol->TxNpassiveStray;
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

  n = pol->TxNpassiveStray;
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

    // check whether we have a GX command ready to send
    if (command[0] == '\0')
    {
      ndiSerialSleep(pol->SerialDevice, 20);
      ndiMutexUnlock(pol->ThreadMutex);
      continue;
    }

    // flush the input buffer, because anything that we haven't read
    //   yet is garbage left over by a previously failed command
    ndiSerialFlush(pol->SerialDevice, NDI_IFLUSH);

    // send the command to the Measurement System
    i = strlen(command);
    if (errorCode == 0)
    {
      m = ndiSerialWrite(pol->SerialDevice, command, i);
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
      m = ndiSerialRead(pol->SerialDevice, reply, 2047);
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