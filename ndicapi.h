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

/*! \file ndicapi.h
  This file contains a complete C interface to the Northern Digital
  Combined API.
*/

#ifndef NDICAPI_H
#define NDICAPI_H

#include "ndicapiExport.h"
#include "ndicapi_serial.h"
#include "ndicapi_socket.h"
#include "ndicapi_thread.h"

#include <stdarg.h>
#include <limits.h>

#define NDICAPI_MAJOR_VERSION 3
#define NDICAPI_MINOR_VERSION 7

// Max tools is 12 active plus 9 passive, so 24 is a safe number
// (note that we are only counting the number of handles that can
// be simultaneously occupied)
#define NDI_MAX_HANDLES 24

//----------------------------------------------------------------------------
// Structure for holding ndicapi data.
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
  unsigned short BxReplyLength;
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

  // BX2 command reply data
  unsigned short Bx2GBFVersion;
  unsigned short Bx2ComponentCount;
  unsigned int Bx2ReplyLength;
  unsigned char Bx2FrameType;
  unsigned int Bx2FrameNumber;
  float Bx2Transforms[NDI_MAX_HANDLES][8];
  unsigned char Bx2FrameSequenceIndex;
  unsigned char Bx2Timestamp[8];
  unsigned int Bx2HandleCount;
  unsigned short Bx2Handles[NDI_MAX_HANDLES];
  unsigned short Bx2HandlesStatus[NDI_MAX_HANDLES];
  bool Bx2HandleAveragingEnabled[NDI_MAX_HANDLES];
  unsigned short Bx2SystemAlerts[256][2]; // Type and value together
  unsigned int Bx2SystemAlertsCount;

  unsigned short Bx2_3DMarkerCount[NDI_MAX_HANDLES];
  char Bx2_3DMarkerStatus[NDI_MAX_HANDLES][20];
  float Bx2_3DMarkerPosition[NDI_MAX_HANDLES][20][3]; // a tool can have up to 20 markers
};

typedef struct ndicapi ndicapi;

/*=====================================================================*/
/*! \defgroup NDIMethods Core Interface Methods

   These are the primary set of methods for communicating with
   tracking devices from NDI that follow the NDI Combined Application
   Programmers' Interface.
*/

#ifdef __cplusplus
extern "C" {
#endif
/*! \ingroup NDIMethods
  This function will return the name of the \em i th serial port device,
  i.e. \em i = 0 gives "COM1:" on Windows and "/dev/ttyS0" on linux.

  \param i  integer between 0 and 3

  \return   the name of a serial port device or 0

  If \em i is too large, the return value is zero.
*/
ndicapiExport const char* ndiSerialDeviceName(int i);

/*! \ingroup NDIMethods
  Probe for an NDI device on the specified serial port device.
  This will perform the following steps:
  -# save the status of the device (baud rate etc)
  -# open the device at 9600 baud, 8 data bits, no parity, 1 stop bit
  -# send an "INIT:" command and check for the expected reply
  -# if the "INIT:" failed, send a serial break and re-try the "INIT:"
  -# if the "INIT:" succeeds, send "VER:0" and check for response.
  -# restore the device to its previous state and close the device.

  \param device    name of a valid serial port device
  \param checkDSR  whether or not to perform a DSR check

  \return one of:
  - NDI_OKAY         -  probe was successful
  - NDI_OPEN_ERROR   -  couldn't open the device
  - NDI_BAD_COMM     -  device file is probably not a serial port
  - NDI_WRITE_ERROR  -  I/O error while writing to the device
  - NDI_READ_ERROR   -  I/O error while reading from device
  - NDI_TIMEOUT      -  timeout while waiting for data
  - NDI_PROBE_FAIL   -  the device found was not an NDI device
*/
ndicapiExport int ndiSerialProbe(const char* device, bool checkDSR);

/*! \ingroup NDIMethods
  Open communication with the NDI device on the specified
  serial port device.  This also sets the serial port parameters to
  (9600 baud, 8N1, no handshaking).

  \param device  name of a valid serial port device

  \return 1 if an NDI device was found on the specified port, 0 otherwise

*/
ndicapiExport ndicapi* ndiOpenSerial(const char* device);

/*! \ingroup NDIMethods
Open communication with the NDI device on the specified
network host and port.

\param hostname URL of the NDI device
\param port Port of the NDI device

\return 1 if an NDI device was found at the specified address, 0 otherwise

*/
ndicapiExport ndicapi* ndiOpenNetwork(const char* hostname, int port);

/*! \ingroup NDIMethods
  Close communication with the NDI device.  You should send
  a "COMM:00000" command before you close communication so that you
  can resume communication on a subsequent call to ndiOpen() without
  having to reset the NDI device.
*/
ndicapiExport void ndiCloseSerial(ndicapi* pol);

/*! \ingroup NDIMethods
Close communication with the NDI device.
*/
ndicapiExport void ndiCloseNetwork(ndicapi* pol);

/*! \ingroup NDIMethods
  Set up multithreading to increase efficiency.

  \param pol    valid NDI device handle
  \param mode   true to turn on threading, false to turn off threading

  It can take milliseconds or even tens of milliseconds for the tracking
  system to return the transform data after a TX or GX command.  During
  this time, the application normally sits idle.

  One way to avoid this idle time is to spawn a separate thread to send
  the GX/TX/BX commands to the device and wait for the replies.
  This allows the main application thread to continue to execute while
  the other thread is waiting for a reply from the device.

  When the thread mode is set, every time the application sends a GX,
  TX or BX command to the device during tracking mode the
  spawned thread will automatically begin resending the GX/TX/BX
  command as fast as possible.
  Then when the application sends the next GX/TX/BX command, the
  thread will simply send the application the most recent reply
  instead of forcing the application to wait through a full command/reply
  cycle.

  If the application changes the reply mode for the GX, TX or BX command,
  then the thread will begin sending commands with the new reply format.
*/
ndicapiExport void ndiSetThreadMode(ndicapi* pol, bool mode);

/*! \ingroup NDIMethods
  Send a command to the device using a printf-style format string.

  \param pol    valid NDI device handle
  \param format a printf-style format string
  \param ...    format arguments as per the format string

  \return       the text reply from the device with the
                CRC chopped off

  The standard format of an NDI API command is, for example, "INIT:" or
  "PENA:AD".  A CRC value and a carriage return will be appended to the
  command before it is sent to the device.

  This function will automatically recognize certain commands and behave
  accordingly:
  - NULL - A serial break will be sent to the device.
  - "COMM:" - After the COMM is sent, the host computer serial port is
           adjusted to match the device.
  - "INIT:" - After the INIT is sent, communication will be paused
           for 100ms.
  - "PHSR:" - The information returned by the PHSR command is stored and can
           be retrieved though the ndiGetPHSR() functions.
  - "PHINF:" - The information returned by the PHINF command is stored and can
           be retrieved though the ndiGetPHINF() functions.
  - "TX:"   - The information returned by the GX command is stored and can
           be retrieved though the ndiGetTX() functions.
  - "GX:"   - The information returned by the GX command is stored and can
           be retrieved though the ndiGetGX() functions.
  - "PSTAT:" - The information returned by the PSTAT command is stored and
           can be retrieved through one of the ndiGetPSTAT() functions.
  - "SSTAT:" - The information returned by the SSTAT command is stored and
           can be retrieved through one of the ndiGetSSTAT() functions.
  - "IRCHK:" - The information returned by the IRCHK command is stored and
           can be retrieved through the ndiGetIRCHK() functions.

  <p>The ndiGetError() function can be used to check whether an error
  occured or, alternatively, ndiSetErrorCallback() can be used to set
  a function that will be called whenever an error occurs.

  For convenience, there is a set of macros for sending commands to the
  devices.  These are listed in \ref NDIMacros.
*/
ndicapiExport char* ndiCommand(ndicapi* pol, const char* format, ...);

/*! \ingroup NDIMethods
  This function is identical in behaviour to ndiCommand(), except that
  it accepts a va_list instead of an argument list.  The use of va_list
  is described in the standard C header file "stdarg.h".
*/
ndicapiExport char* ndiCommandVA(ndicapi* pol, const char* format, va_list ap);

/*! \ingroup NDIMethods
  Error callback type for use with ndiSetErrorCallback().
*/
typedef void (*NDIErrorCallback)(int errnum, char* description, void* userdata);

/*! \ingroup NDIMethods
  Set a function that will be called each time an error occurs.

  \param pol       valid NDI device handle
  \param callback  a callback with the following signature:\n
    void callback(int errnum, char *error_description, void *userdata)
  \param userdata  data to send to the callback each time it is called

  The callback can be set to NULL to erase a previous callback.
*/
ndicapiExport void ndiSetErrorCallback(ndicapi* pol, NDIErrorCallback callback,
                                       void* userdata);

/*! \ingroup NDIMethods
Log current state of NDICAPI to string

\param outInformation A LF delimited string describing the state of each variable in the system in the format "<variableName>=<variableValue><LF><variableName>=..."
*/
ndicapiExport void ndiLogState(ndicapi* pol, char outInformation[USHRT_MAX]);

/*! \ingroup NDIMethods
Set the socket timeout

\param timeoutMsec the timeout in milliseconds
*/
ndicapiExport void ndiTimeoutSocket(ndicapi* pol, int timeoutMsec);

/*=====================================================================*/
/*! \defgroup NDIMacros Command Macros
  These are a set of macros that send commands to the device via
  ndiCommand().

  The ndiGetError() function can be used to determine whether an error
  occurred.

  The return value is a terminated string containing the reply from the
  device with the CRC and final carriage return chopped off.
*/

/*\{*/
/*!
  Get the 3D position and an error value for a single marker on a
  tool.  The mode should be set to 1 or 3.  This command is only
  available in diagnostic mode.

  \param ph    valid port handle in the range 0x01 to 0xFF
  \param mode  see NDI documentation

  The IRED command should be used to set up a marker firing signature
  before this command is called.
*/
#define ndi3D(p,ph,mode) ndiCommand((p),"3D:%02X%d",(ph),(mode))

/*!
  Cause the device to beep.

  \param n   the number of times to beep, an integer between 1 and 9

  A reply of "0" means that the device is already beeping
  and cannot service this beep request.

  This command can be used in tracking mode.
*/
#define ndiBEEP(p,n) ndiCommand((p),"BEEP:%i",(n))

/*!
  Change the device communication parameters.  The host parameters
  will automatically be adjusted to match.  If the specified baud rate is
  not supported by the serial port, then the error code will be set to
  NDI_BAD_COMM and the device will have to be reset before
  communication can continue.  Most modern UNIX systems accept all baud
  rates except 14400, and Windows systems support all baud rates.

  \param baud one of NDI_9600, NDI_14400, NDI_19200, NDI_38400, NDI_57600,
         NDI_115200
  \param dps  should usually be NDI_8N1, the most common mode
  \param h   one of NDI_HANDSHAKE or NDI_NOHANDSHAKE
*/
#define ndiCOMM(p,baud,dps,h) ndiCommand((p),"COMM:%d%03d%d",(baud),(dps),(h))

/*!
  Put the device into diagnostic mode.  This must be done prior
  to executing the IRCHK() command.  Diagnostic mode is only useful on
  the POLARIS
*/
#define ndiDSTART(p) ndiCommand((p),"DSTART:")

/*!
  Take the device out of diagnostic mode.
*/
#define ndiDSTOP(p) ndiCommand((p),"DSTOP:")

/*!
  Request tracking information from the system.  This command is
  only available in tracking mode.  Please note that this command has
  been deprecated in favor of the TX command.

  \param mode a reply mode containing the following bits:
  - NDI_XFORMS_AND_STATUS  0x0001 - transforms and status
  - NDI_ADDITIONAL_INFO    0x0002 - additional tool transform info
  - NDI_SINGLE_STRAY       0x0004 - stray active marker reporting
  - NDI_FRAME_NUMBER       0x0008 - frame number for each tool
  - NDI_PASSIVE            0x8000 - include information for ports 'A', 'B', 'C'
  - NDI_PASSIVE_EXTRA      0x2000 - with NDI_PASSIVE, ports 'A' to 'I'
  - NDI_PASSIVE_STRAY      0x1000 - stray passive marker reporting

  <p>The GX command with the appropriate reply mode is used to update the
  data that is returned by the following functions:
  - int \ref ndiGetGXTransform(ndicapi *pol, int port, double transform[8])
  - int \ref ndiGetGXPortStatus(ndicapi *pol, int port)
  - int \ref ndiGetGXSystemStatus(ndicapi *pol)
  - int \ref ndiGetGXToolInfo(ndicapi *pol, int port)
  - int \ref ndiGetGXMarkerInfo(ndicapi *pol, int port, int marker)
  - int \ref ndiGetGXSingleStray(ndicapi *pol, int port, double coord[3])
  - unsigned long \ref ndiGetGXFrame(ndicapi *pol, int port)
  - int \ref ndiGetGXNumberOfPassiveStrays(ndicapi *pol)
  - int \ref ndiGetGXPassiveStray(ndicapi *pol, int i, double coord[3])
*/
#define ndiGX(p,mode) ndiCommand((p),"GX:%04X",(mode))

/*!
  Initialize the device.  The device must be
  initialized before any other commands are sent.
*/
#define ndiINIT(p) ndiCommand((p),"INIT:")

/*!
  Set the POLARIS infrared firing rate to 20 Hz = 0, 30 Hz = 1, 20 Hz = 2
*/
#define ndiIRATE(p,rate) ndiCommand((p),"IRATE:%d",(rate))

/*!
  Check for sources of environmental infrared.  This command is only
  valid in diagnostic mode after an IRINIT command.

  \param mode  reply mode bits:
  - NDI_DETECTED   0x0001 - return '1' if IR detected, else '0'
  - NDI_SOURCES    0x0002 - locations of up to 20 sources per camera

  <p>The IRCHK command is used to update the information returned by the
  ndiGetIRCHKDetected() and ndiGetIRCHKSourceXY() functions.
*/
#define ndiIRCHK(p,mode) ndiCommand((p),"IRCHK:%04X",(mode))

/*!
  Set the infrared firing signature for the specified port.  This
  command is only available in diagnostic mode.

  \param ph    valid port handle in the range 0x01 to 0xFF
  \param sig   a 32-bit integer whose bits specify what IREDS to fire
*/
#define ndiIRED(p,ph,sig) ndiCommand((p),"IRED:%02X%08X",(ph),(sig))

/*!
  Initialize the diagnostic environmental infrared checking system.
  This command must be called prior to using the IRCHK command.
*/
#define ndiIRINIT(p) ndiCommand((p),"IRINIT:")

/*!
  Set a tool LED to a particular state.

  \param ph     valid port handle in the range 0x01 to 0xFF
  \param led    an integer between 1 and 3
  \param state  desired state: NDI_BLANK 'B', NDI_FLASH 'F' or NDI_SOLID 'S'

  This command can be used in tracking mode.
*/
#define ndiLED(p,ph,led,s) ndiCommand((p),"LED:%02X%d%c",(ph),(led),(s))

/*!
  Disable transform reporting on the specified port handle.

  \param ph valid port handle in the range 0x01 to 0xFF
*/
#define ndiPDIS(p,ph) ndiCommand((p),"PDIS:%02X",(ph))

/*!
  Enable transform reporting on the specified port handle.

  \param ph valid port handle in the range 0x01 to 0xFF
  \param mode one of NDI_STATIC 'S', NDI_DYNAMIC 'D' or NDI_BUTTON_BOX 'B'
*/
#define ndiPENA(p,ph,mode) ndiCommand((p),"PENA:%02X%c",(ph),(mode))

/*!
  Specify a POLARIS tool faces to use for tracking

  \param ph valid port handle in the range 0x01 to 0xFF
  \param tf tool face mask in the range 0x01 to 0xFF
*/
#define ndiPFSEL(p,ph,tf) ndiCommand((p),"PFSEL:%02X%02X",(ph),(tf))

/*!
  Free the specified port handle.

  \param ph valid port handle in the range 0x01 to 0xFF
*/
#define ndiPHF(p,ph) ndiCommand((p),"PHF:%02X",(ph))

/*!
  Ask the device for information about a tool handle.

  \param format  a reply format mode composed of the following bits:
  - NDI_BASIC           0x0001 - get port status and basic tool information
  - NDI_TESTING         0x0002 - get current test for active tools
  - NDI_PART_NUMBER     0x0004 - get a 20 character part number
  - NDI_ACCESSORIES     0x0008 - get a summary of the tool accessories
  - NDI_MARKER_TYPE     0x0010 - get the tool marker type
  - NDI_PORT_LOCATION   0x0020 - get the physical port location
  - NDI_GPIO_STATUS     0x0040 - get AURORA GPIO status

  <p>The use of the PHINF command with the appropriate reply format updates
  the information returned by the following commands:
  - int \ref ndiGetPHINFPortStatus(ndicapi *pol)
  - int \ref ndiGetPHINFToolInfo(ndicapi *pol, char information[30])
  - unsigned long \ref ndiGetPHINFCurrentTest(ndicapi *pol)
  - int \ref ndiGetPHINFAccessories(ndicapi *pol)
  - int \ref ndiGetPHINFMarkerType(ndicapi *pol)

  <p>This command is not available during tracking mode.
*/
#define ndiPHINF(p,ph,format) ndiCommand((p),"PHINF:%02X%04X",(ph),(format))

/*!
  Requeset a port handle given specific tool criteria.

  \param  num    8-digit device number or wildcard "********"
  \param  sys    system type TIU "0" or AURORA SCU "1" or wildcard "*"
  \param  tool   wired "0" or wireless "1" or wildcard "*"
  \param  port   wired "01" to "10", wireless "0A" to "0I" or wildcard "**"
  \param  chan   AURORA tool channel "00" or "01" or wildcard "**"

  <p>The use of the PHRQ command updates the information returned by the
  following commands:
  - int \ref ndiGetPHRQHandle(ndicapi *pol)
*/
#define ndiPHRQ(p,num,sys,tool,port,chan) ndiCommand((p),"PHRQ:%-8.8s%1.1s%1.1s%2.2s%2.2s",(num),(sys),(tool),(port),(chan))

/*!
  List the port handles.

  \param mode   the reply mode:
  - NDI_ALL_HANDLES            0x00 - return all handles
  - NDI_STALE_HANDLES          0x01 - only handles waiting to be freed
  - NDI_UNINITIALIZED_HANDLES  0x02 - handles needing initialization
  - NDI_UNENABLED_HANDLES      0x03 - handles needing enabling
  - NDI_ENABLED_HANDLES        0x04 - handles that are enabled

  <p>The use of the PHSR command with the appropriate reply format updates
  the information returned by the following commands:
  - int \ref ndiGetPHSRNumberOfHandles(ndicapi *pol)
  - int \ref ndiGetPHSRHandle(ndicapi *pol, int i)
  - int \ref ndiGetPHSRInformation(ndicapi *pol, int i)

  <p>This command is not available during tracking mode.
*/
#define ndiPHSR(p,mode) ndiCommand((p),"PHSR:%02X",(mode))

/*!
  Initialize the tool on the specified port handle.

  \param ph valid port handle in the range 0x01 to 0xFF
*/
#define ndiPINIT(p,ph) ndiCommand((p),"PINIT:%02X",(ph))

/*! Read the tool SROM. Use ndiHexDecode() to convert reply to binary. */
#define ndiPPRD(p,ph,addr) ndiCommand((p),"PPRD:%02X%04X",(ph),(addr))

/*! Write the tool SROM. Use ndiHexEncode() to convert data to a hex string. */
#define ndiPPWR(p,ph,a,x) ndiCommand((p),"PPWR:%02X%04X%.128s",(ph),(a),(x))

/*! Select an SROM target by ID. */
#define ndiPSEL(p,ph,id) ndiCommand((p),"PSEL:%02X%s",(ph),(id))

/*!
  Set the three GPIO wire states for an AURORA tool.
  The states available are 'N' (no change), 'S' (solid on),
  'P' (pulse) and 'O' (off).

  \param ph  valid port handle in the range 0x01 to 0xFF
  \param a   GPIO 1 state
  \param b   GPIO 2 state
  \param c   GPIO 3 state
*/
#define ndiPSOUT(p,ph,a,b,c) ndiCommand((p),"PSOUT:%02X%c%c%c",(ph),(a),(b),(c))

/*! Search for available SROM targets. */
#define ndiPSRCH(p,ph) ndiCommand((p),"PSRCH:%02X",(ph))

/*!
  Ask for information about the tool ports.  This command has been
  deprecated in favor of the PHINF command.

  \param format  a reply format mode composed of the following bits:
  - NDI_BASIC           0x0001 - get port status and basic tool information
  - NDI_TESTING         0x0002 - get current test for active tools
  - NDI_PART_NUMBER     0x0004 - get a 20 character part number
  - NDI_ACCESSORIES     0x0008 - get a summary of the tool accessories
  - NDI_MARKER_TYPE     0x0010 - get the tool marker type
  - NDI_PASSIVE         0x8000 - include information for ports 'A', 'B', 'C'
  - NDI_PASSIVE_EXTRA   0x2000 - with NDI_PASSIVE, ports 'A' to 'I'

  <p>The use of the PSTAT command with the appropriate reply format updates
  the information returned by the following commands:
  - int \ref ndiGetPSTATPortStatus(ndicapi *pol, int port)
  - int \ref ndiGetPSTATToolInfo(ndicapi *pol, int port,
                                   char information[30])
  - unsigned long \ref ndiGetPSTATCurrentTest(ndicapi *pol, int port)
  - int \ref ndiGetPSTATAccessories(ndicapi *pol, int port)
  - int \ref ndiGetPSTATMarkerType(ndicapi *pol, int port)

  <p>This command is not available during tracking mode.
*/
#define ndiPSTAT(p,format) ndiCommand((p),"PSTAT:%04X",(format))

/*! Read the user part of the tool SROM. Use ndiHexDecode() convert reply. */
#define ndiPURD(p,ph,addr) ndiCommand((p),"PURD:%02X%04X",(ph),(addr))

/*! Write the user part of the tool SROM. Use ndiHexEncode() to convert data.*/
#define ndiPUWR(p,ph,a,x) ndiCommand((p),"PPWR:%02X%04X%.128s",(ph),(a),(x))

/*!
  Clear the virtual SROM for the specified port.  For a passive tool,
  this is equivalent to unplugging the tool.  This command has been
  deprecated in favor of PHF.

  \param port one of '1', '2', '3' or 'A' to 'I'
*/
#define ndiPVCLR(p,port) ndiCommand((p),"PVCLR:%c",(port))

/*!
  Set the virtual TOOL IN PORT and non-POLARIS tool signals for this port.
  This command has been deprecated in favor of PHRQ.

  \param port one of '1', '2', '3' or 'A' to 'I'
  \param tip  one of 0 (no tool in port) or 1 (tool in port)
  \param type one of 0 (non-POLARIS tool) or 1 (POLARIS tool)
*/
#define ndiPVTIP(p,port,tip,np) ndiCommand((p),"PVTIP:%c%d%d",(port),(tip),(np))

/*!
  Write to a virtual SROM address on the specified port handle.
  The ndiPVWRFromFile() function provides a more convenient means
  of uploading tool descriptions.

  \param ph valid port handle in the range 0x01 to 0xFF
  \param a an address between 0x0000 and 0x07C0
  \param x 64-byte data array encoded as a 128-character hexidecimal string

  The ndiHexEncode() function can be used to encode the data into
  hexidecimal.
*/
#define ndiPVWR(p,ph,a,x) ndiCommand((p),"PVWR:%02X%04X%.128s",(ph),(a),(x))

/*!
  Send a serial break to reset the device.  If the reset was not
  successful, the error code will be set to NDI_RESET_FAIL.
*/
#define ndiRESET(p) ndiCommand((p),NULL)

/*!
  Get a feature list for this device.

  \param mode  the desired reply mode
  - 0x00 - 32-bit feature summary encoded as 8 hexidecimal digits
  - 0x01 - the number of active tool ports as a single digit
  - 0x02 - the number of passive tool ports as a single digit
  - 0x03 - list of volumes available (see NDI documentation)
  - 0x04 - the number of ports that support tool-in-port current sensing

  <p>The feature summary bits are defined as follow:
  - 0x00000001 - active tool ports are available
  - 0x00000002 - passive tool ports are available
  - 0x00000004 - multiple volumes are available
  - 0x00000008 - tool-in-port current sensing is available
*/
#define ndiSFLIST(p,mode) ndiCommand((p),"SFLIST:%02X",(mode))

/*!
  Request status information from the device.

  \param format  a reply format mode composed of the following bits:
  - NDI_CONTROL  0x0001 - control processor information
  - NDI_SENSORS  0x0002 - sensor processors
  - NDI_TIU      0x0004 - TIU processor

  <p>The use of the SSTAT command with the appropriate reply format updates
  the information returned by the following commands:
  - int \ref ndiGetSSTATControl(ndicapi *pol)
  - int \ref ndiGetSSTATSensors(ndicapi *pol)
  - int \ref ndiGetSSTATTIU(ndicapi *pol)

  <p>This command is not available during tracking mode.
*/
#define ndiSSTAT(p,format) ndiCommand((p),"SSTAT:%04X",(format))

/*! Perform a current test on this port.  */
#define ndiTCTST(p,ph) ndiCommand((p),"TCTST:%02X",(ph))

/*!
  Put the device into tracking mode.
*/
#define ndiTSTART(p) ndiCommand((p),"TSTART:")

/*!
  Take the device out of tracking mode.
*/
#define ndiTSTOP(p) ndiCommand((p),"TSTOP:")

/*! Set default tool configuration prior to testing. */
#define ndiTTCFG(p,ph) ndiCommand((p),"TTCFG:%02X",(ph))

/*!
  Request tracking information from the device.  This command is
  only available in tracking mode.

  \param mode a reply mode containing the following bits:
  - NDI_XFORMS_AND_STATUS  0x0001 - transforms and status
  - NDI_ADDITIONAL_INFO    0x0002 - additional tool transform info
  - NDI_SINGLE_STRAY       0x0004 - stray active marker reporting
  - NDI_PASSIVE_STRAY      0x1000 - stray passive marker reporting

  <p>The TX command with the appropriate reply mode is used to update the
  data that is returned by the following functions:
  - int \ref ndiGetTXTransform(ndicapi *pol, int ph, double transform[8])
  - int \ref ndiGetTXPortStatus(ndicapi *pol, int ph)
  - unsigned long \ref ndiGetTXFrame(ndicapi *pol, int ph)
  - int \ref ndiGetTXToolInfo(ndicapi *pol, int ph)
  - int \ref ndiGetTXMarkerInfo(ndicapi *pol, int ph, int marker)
  - int \ref ndiGetTXSingleStray(ndicapi *pol, int ph, double coord[3])
  - int \ref ndiGetTXNumberOfPassiveStrays(ndicapi *pol)
  - int \ref ndiGetTXPassiveStray(ndicapi *pol, int i, double coord[3])
  - int \ref ndiGetTXSystemStatus(ndicapi *pol)
*/
#define ndiTX(p,mode) ndiCommand((p),"TX:%04X",(mode))

/*!
  Get a string that describes the device firmware version.

  \param n   the processor to get the firmware revision of:
  - 0 - control firmware
  - 1 - left sensor firmware
  - 2 - right sensor firmware
  - 3 - TIU firmware
  - 4 - control firmware with enhanced versioning
*/
#define ndiVER(p,n) ndiCommand((p),"VER:%d",(n))

/*!
  Select from the different calibrated operating volumes available to
  the device.

  \param n  the volume to select, see ndiSFLIST()
*/
#define ndiVSEL(p,n) ndiCommand((p),"VSEL:%d",(n))

/*!
  Write data from a ROM file into the virtual SROM for the specified port.

  \param pol       valid NDI device handle
  \param ph        valid port handle in the range 0x01 to 0xFF
  \param filename  file to read the SROM data from

  \return  NDI_OKAY if the write was successful

  If the return value is not NDI_OKAY but ndiGetError() returns NDI_OKAY,
  then the ROM file could not be read and no information was written
  to the device.

  This function uses the PVWR command to write the SROM.  The total size
  of the virtual SROM is 1024 bytes.  If the file is shorter than this,
  then zeros will be written to the remaining space in the SROM.
*/
ndicapiExport int ndiPVWRFromFile(ndicapi* pol, int ph, char* filename);
/*\}*/

/*=====================================================================*/
/*! \defgroup GetMethods Get Methods
  These functions are used to retrieve information that has been stored in
  the ndicapi object.
  None of these functions communicate with the device itself,
  and none of them cause the error indicator to be set.

  The DeviceName and DeviceHandle are set when the device is
  first opened by ndiOpen().

  All other information is set by sending the appropriate command to
  the device through ndiCommand() or through one of the command
  macros. The information persists until the next time the command is sent.

  For example, if the TX command is sent to the device
  then the information returned by the methods that start with
  GetTX will be set and will subsequently remain unchanged until the next
  TX command is sent.
*/

/*! \ingroup GetMethods
  Get error code from the last command.  An error code of NDI_OKAY signals
  that no error occurred.  The error codes are listed in \ref ErrorCodes.
*/
ndicapiExport int ndiGetError(ndicapi* pol);

/*! \ingroup GetMethods
Get error code from the last socket command.  This provides more detailed information
after calling ndiGetError(...)
*/
ndicapiExport int ndiGetSocketError(ndicapi* pol);

/*! \ingroup GetMethods
  Get the name of the serial port device that the device is
  attached to.
*/
ndicapiExport char* ndiGetSerialDeviceName(ndicapi* pol);

/*! \ingroup GetMethods
Get the device handle for the serial port that the device is
attached to.  This device handle is the value returned by the UNIX open()
function or the Win32 CreateFile() function.
*/
ndicapiExport NDIFileHandle ndiGetDeviceHandle(ndicapi* pol);

/*! \ingroup GetMethods
Get the socket that the device is attached to.
*/
ndicapiExport NDISocketHandle ndiGetSocket(ndicapi* pol);

/*! \ingroup GetMethods
Get the name of the host the device is attached to.
*/
ndicapiExport char* ndiGetHostname(ndicapi* pol);

/*! \ingroup GetMethods
Get the port that the device is attached to.
*/
ndicapiExport int ndiGetPort(ndicapi* pol);

/*! \ingroup GetMethods
  Check the current theading mode.  The default is 0 (no multithreading).
*/
ndicapiExport int ndiGetThreadMode(ndicapi* pol);

/*! \ingroup GetMethods
  Get the current error callback function, or NULL if there is none.
*/
ndicapiExport NDIErrorCallback ndiGetErrorCallback(ndicapi* pol);

/*! \ingroup GetMethods
  Get the current error callback data, or NULL if there is none.
*/
ndicapiExport void* ndiGetErrorCallbackData(ndicapi* pol);

/*! \ingroup GetMethods
  Get the port handle returned by a PHRQ command.

  \param pol       valid NDI device handle

  \return  a port handle between 0x01 and 0xFF

  <p>An SROM can be written to the port handle wit the PVWR command.
*/
ndicapiExport int ndiGetPHRQHandle(ndicapi* pol);

/*! \ingroup GetMethods
  Get the number of port handles as returned by a PHSR command.

  \param pol       valid NDI device handle

  \return  an integer, the maximum possible value is 255
*/
ndicapiExport int ndiGetPHSRNumberOfHandles(ndicapi* pol);

/*! \ingroup GetMethods
  Get one of the port handles returned by a PHSR command.

  \param pol       valid NDI device handle
  \param i         a value between 0 and \em n where \n is the
                   value returned by ndiGetPHSRNumberOfHandles().

  \return  a port handle between 0x01 and 0xFF

  <p>The PHINF command can be used to get detailed information about the
   port handle.
*/
ndicapiExport int ndiGetPHSRHandle(ndicapi* pol, int i);

/*! \ingroup GetMethods
  Get the information for a port handle returned by a PHSR command.

  \param pol       valid NDI device handle
  \param i         a value between 0 and \em n where \n is the
                   value returned by ndiGetPHSRNumberOfHandles().

  \return  a 12-bit bitfield where the following bits are defined:
  - NDI_TOOL_IN_PORT        0x01 - there is a tool in the port
  - NDI_SWITCH_1_ON         0x02 - button 1 is pressed
  - NDI_SWITCH_2_ON         0x04 - button 2 is pressed
  - NDI_SWITCH_3_ON         0x08 - button 3 is pressed
  - NDI_INITIALIZED         0x10 - tool port has been initialized
  - NDI_ENABLED             0x20 - tool port is enabled for tracking
  - NDI_CURRENT_DETECT      0x80 - tool sensed through current detection

  <p>The PHINF command can be used to get detailed information about the
   port handle.
*/
ndicapiExport int ndiGetPHSRInformation(ndicapi* pol, int i);

/*! \ingroup GetMethods
  Get the 8-bit status value for the port handle.

  \param pol       valid NDI device handle

  \return a an integer composed of the following bits:
  - NDI_TOOL_IN_PORT        0x01 - there is a tool in the port
  - NDI_SWITCH_1_ON         0x02 - button 1 is pressed
  - NDI_SWITCH_2_ON         0x04 - button 2 is pressed
  - NDI_SWITCH_3_ON         0x08 - button 3 is pressed
  - NDI_INITIALIZED         0x10 - tool port has been initialized
  - NDI_ENABLED             0x20 - tool port is enabled for tracking
  - NDI_CURRENT_DETECT      0x80 - tool sensed through current detection

  <p>The return value is updated only when a PHINF command is sent with
  the NDI_BASIC (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFPortStatus(ndicapi* pol);

/*! \ingroup GetMethods
  Get a 31-byte string describing the tool.

  \param piol         valid NDI device handle
  \param information array that information is returned in (the
                     resulting string is not null-terminated)

  \return  one of:
  - NDI_OKAY - information was returned
  - NDI_UNOCCUPIED - port is unoccupied or no information is available

  <p>The returned string will not be null-terminated by default.  You
  must set information[31] to 0 in order to terminate the string.
  If the port is unoccupied then the contents of the \em information
  string are undefined.

  The information is updated only when a PHINF command is sent with
  the NDI_BASIC (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFToolInfo(ndicapi* pol, char information[31]);

/*! \ingroup GetMethods
  Return the results of a current test on the IREDS on an active
  POLARIS tool.

  \param  ts          valid NDI device handle

  \return 32-bit integer (see NDI documentation)

  The information is updated only when a PHINF command is sent with
  the NDI_TESTING (0x0002) bit set in the reply mode.
*/
ndicapiExport unsigned long ndiGetPHINFCurrentTest(ndicapi* pol);

/*! \ingroup GetMethods
  Get a 20-byte string that contains the part number of the tool.

  \param pol         valid NDI device handle
  \param part        array that part number is returned in (the
                     resulting string is not null-terminated)

  \return  one of:
  - NDI_OKAY - information was returned
  - NDI_UNOCCUPIED - port is unoccupied or no information is available

  <p>If a terminated string is required, then set part[20] to '\0'
  before calling this function.

  The information is updated only when a PHINF command is sent with
  the NDI_PART_NUMBER (0x0004) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFPartNumber(ndicapi* pol, char part[20]);

/*! \ingroup GetMethods
  Get the 8-bit value specifying the tool accessories.

  \param pol       valid NDI device handle

  \return a an integer composed of the following bits:
  - NDI_TOOL_IN_PORT_SWITCH   0x01  - tool has tool-in-port switch
  - NDI_SWITCH_1              0x02  - tool has button 1
  - NDI_SWITCH_2              0x04  - tool has button 2
  - NDI_SWITCH_3              0x08  - tool has button 3
  - NDI_TOOL_TRACKING_LED     0x10  - tool has tracking LED
  - NDI_LED_1                 0x20  - tool has LED 1
  - NDI_LED_2                 0x40  - tool has LED 2
  - NDI_LED_3                 0x80  - tool has LED 3

  <p>If there is no available information for the specified port, or if
  an illegal port specifier value is used, the return value is zero.

  The return value is updated only when a PHINF command is sent with
  the NDI_ACCESSORIES (0x0008) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFAccessories(ndicapi* pol);

/*! \ingroup GetMethods
  Get an 8-bit value describing the marker type for the tool.
  The low three bits descibe the wavelength, and the high three
  bits are the marker type code.

  \param pol       valid NDI device handle

  \return  see NDI documentation for more information:
  - low bits:
    - NDI_950NM            0x00
    - NDI_850NM            0x01
  - high bits:
    - NDI_NDI_ACTIVE       0x08
    - NDI_NDI_CERAMIC      0x10
    - NDI_PASSIVE_ANY      0x20
    - NDI_PASSIVE_SPHERE   0x28
    - NDI_PASSIVE_DISC     0x30

  <p>The return value is updated only when a PHINF command is sent with
  the NDI_MARKER_TYPE (0x0010) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFMarkerType(ndicapi* pol);

/*! \ingroup GetMethods
  Get a 14-byte description of the physical location of the tool
  on the system.

  \param pol       valid NDI device handle

  \return  see NDI documentation for more information:
  - 8 chars: device number
  - 1 char: 0 = POLARIS, 1 = AURORA
  - 1 char: 0 = wired, 1 = wireless
  - 2 chars: port number 01-12 or 0A-0I
  - 2 chars: hexidecimal channel number

  <p>The return value is updated only when a PHINF command is sent with
  the NDI_PORT_LOCATION_TYPE (0x0020) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFPortLocation(ndicapi* pol, char location[14]);

/*! \ingroup GetMethods
  Get the 8-bit GPIO status for this tool.

  \param pol       valid NDI device handle

  \return  an 8-bit integer, see NDI documentation for more information.

  <p>The return value is updated only when a PHINF command is sent with
  the NDI_GPIO_STATUS (0x0040) bit set in the reply mode.
*/
ndicapiExport int ndiGetPHINFGPIOStatus(ndicapi* pol);

/*! \ingroup GetMethods
  Get the transformation for the specified port.
  The first four numbers are a quaternion, the next three numbers are
  the coodinates in millimetres, and the final number
  is a unitless error estimate.

  \param pol       valid NDI device handle
  \param ph        valid port handle in range 0x01 to 0xFF
  \param transform space for the 8 numbers in the transformation

  \return one of the following:
  - NDI_OKAY if successful
  - NDI_DISABLED if tool port is nonexistent or disabled
  - NDI_MISSING if tool transform cannot be computed

  <p>If NDI_DISABLED or NDI_MISSING is returned, then the values in the
  supplied transform array will be left unchanged.

  The transformations for each of the port handles remain the same
  until the next TX command is sent to device.
*/
ndicapiExport int ndiGetTXTransformf(ndicapi* pol, int portHandle, float transform[8]);
ndicapiExport int ndiGetTXTransform(ndicapi* pol, int portHandle, double transform[8]);

/*! \ingroup GetMethods
  Get the 16-bit status value for the specified port handle.

  \param pol       valid NDI device handle
  \param ph        valid port handle in range 0x01 to 0xFF

  \return status bits or zero if there is no information:
  - NDI_TOOL_IN_PORT        0x0001
  - NDI_SWITCH_1_ON         0x0002
  - NDI_SWITCH_2_ON         0x0004
  - NDI_SWITCH_3_ON         0x0008
  - NDI_INITIALIZED         0x0010
  - NDI_ENABLED             0x0020
  - NDI_OUT_OF_VOLUME       0x0040
  - NDI_PARTIALLY_IN_VOLUME 0x0080

  This information is updated each time that the TX command
  is sent to the device.
*/
ndicapiExport int ndiGetTXPortStatus(ndicapi* pol, int ph);

/*! \ingroup GetMethods
  Get the camera frame number for the latest transform.

  \param pol       valid NDI device handle
  \param ph        valid port handle in range 0x01 to 0xFF

  \return a 32-bit frame number, or zero if no information was available

  This information is updated each time that the TX command
  is sent to the device.
*/
ndicapiExport unsigned long ndiGetTXFrame(ndicapi* pol, int ph);

/*! \ingroup GetMethods
  Get additional information about the tool transformation.

  \param pol       valid NDI device handle
  \param ph        valid port handle in range 0x01 to 0xFF

  \return status bits, or zero if there is no information available
  - NDI_BAD_TRANSFORM_FIT   0x01
  - NDI_NOT_ENOUGH_MARKERS  0x02
  - NDI_TOOL_FACE_USED      0x70 - 3 bits give 8 possible faces

  <p>The tool information is only updated when the TX command is called with
  the NDI_ADDITIONAL_INFO (0x0002) mode bit.
*/
ndicapiExport int ndiGetTXToolInfo(ndicapi* pol, int ph);

/*! \ingroup GetMethods
  Get additional information about the tool markers.

  \param pol       valid NDI device handle
  \param ph        valid port handle in range 0x01 to 0xFF
  \param marker    one of 'A' through 'T' for the 20 markers

  \return status bits, or zero if there is no information available
  - NDI_MARKER_MISSING             0
  - NDI_MARKER_EXCEEDED_MAX_ANGLE  1
  - NDI_MARKER_EXCEEDED_MAX_ERROR  2
  - NDI_MARKER_USED                3

  <p>The tool marker information is only updated when the TX command is
  called with the NDI_ADDITIONAL_INFO (0x0002) mode bit set.
*/
ndicapiExport int ndiGetTXMarkerInfo(ndicapi* pol, int ph, int marker);

/*! \ingroup GetMethods
  Get the coordinates of a stray marker on a wired POLARIS tool.
  This command is only meaningful for tools that have a stray
  marker.

  \param pol       valid NDI device handle
  \param ph        valid port handle in range 0x01 to 0xFF
  \param coord     array to hold the three coordinates

  \return the return value will be one of
  - NDI_OKAY - values returned in coord
  - NDI_DISABLED - port disabled or illegal port specified
  - NDI_MISSING - stray marker is not visible to the device

  <p>The stray marker position is only updated when the GX command is
  called with the NDI_SINGLE_STRAY (0x0004) bit set.
*/
ndicapiExport int ndiGetTXSingleStray(ndicapi* pol, int ph, double coord[3]);

/*! \ingroup GetMethods
  Get the number of passive stray markers detected.

  \param pol       valid NDI device handle
  \return          a number between 0 and 50

  The passive stray marker coordinates are updated when a TX command
  is sent with the NDI_PASSIVE_STRAY (0x1000) bit set in the reply mode.
*/
ndicapiExport int ndiGetTXNumberOfPassiveStrays(ndicapi* pol);

/*! \ingroup GetMethods
  Copy the coordinates of the specified stray marker into the
  supplied array.

  \param pol       valid NDI device handle
  \param i         a number between 0 and 49
  \param coord     array to hold the coordinates
  \return          one of:
  - NDI_OKAY - information was returned in coord
  - NDI_DISABLED - no stray marker reporting is available
  - NDI_MISSING - marker number i is not visible

  <p>Use ndiGetTXNumberOfPassiveStrays() to get the number of stray
  markers that are visible.

  The passive stray marker coordinates are updated when a TX command
  is sent with the NDI_PASSIVE_STRAY (0x1000) bit set in the reply mode.
*/
ndicapiExport int ndiGetTXPassiveStray(ndicapi* pol, int i, double coord[3]);

/*! \ingroup GetMethods
  Get an 16-bit status bitfield for the system.

  \param pol       valid NDI device handle

  \return status bits or zero if there is no information:
  - NDI_COMM_SYNC_ERROR            0x0001
  - NDI_TOO_MUCH_EXTERNAL_INFRARED 0x0002
  - NDI_COMM_CRC_ERROR             0x0004
  - NDI_COMM_RECOVERABLE           0x0008
  - NDI_HARDWARE_FAILURE           0x0010
  - NDI_HARDWARE_CHANGE            0x0020
  - NDI_PORT_OCCUPIED              0x0040
  - NDI_PORT_UNOCCUPIED            0x0080

  <p>The system stutus information is updated whenever the TX command is
  called with the NDI_XFORMS_AND_STATUS (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetTXSystemStatus(ndicapi* pol);

/*! \ingroup GetMethods
  Get the transformation for the specified port.
  The first four numbers are a quaternion, the next three numbers are
  the coodinates in millimetres, and the final number
  is a unitless error estimate.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' or 'A' to 'I'
  \param transform space for the 8 numbers in the transformation

  \return one of the following:
  - NDI_OKAY if successful
  - NDI_DISABLED if tool port is nonexistent or disabled
  - NDI_MISSING if tool transform cannot be computed

  <p>If NDI_DISABLED or NDI_MISSING is returned, then the values in the
  supplied transform array will be left unchanged.

  The transforms for ports '1', '2', '3' are updated whenever the
  GX comand is sent with the NDI_XFORMS_AND_STATUS (0x0001) bit set
  in the reply mode.
  The passive ports 'A', 'B', 'C' are updated if the NDI_PASSIVE (0x8000)
  bit is also set, and ports 'A' though 'I' are updated if both
  NDI_PASSIVE (0x8000) and NDI_PASSIVE_EXTRA (0x2000) are also set.

  The transformation for any particular port will remain unchanged
  until it is updated by a GX command with an appropriate reply mode
  as specified above.
*/
ndicapiExport int ndiGetGXTransform(ndicapi* pol, int port, double transform[8]);

/*! \ingroup GetMethods
  Get the 8-bit status value for the specified port.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' or 'A' to 'I'

  \return status bits or zero if there is no information:
  - NDI_TOOL_IN_PORT        0x01
  - NDI_SWITCH_1_ON         0x02
  - NDI_SWITCH_2_ON         0x04
  - NDI_SWITCH_3_ON         0x08
  - NDI_INITIALIZED         0x10
  - NDI_ENABLED             0x20
  - NDI_OUT_OF_VOLUME       0x40
  - NDI_PARTIALLY_IN_VOLUME 0x80

  The status of the ports is updated according to the same rules as
  specified for ndiGetGXTransform().
*/
ndicapiExport int ndiGetGXPortStatus(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get an 8-bit status bitfield for the system.

  \param pol       valid NDI device handle

  \return status bits or zero if there is no information:
  - NDI_COMM_SYNC_ERROR            0x01
  - NDI_TOO_MUCH_EXTERNAL_INFRARED 0x02
  - NDI_COMM_CRC_ERROR             0x04

  <p>The system stutus information is updated whenever the GX command is
  called with the NDI_XFORMS_AND_STATUS (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetGXSystemStatus(ndicapi* pol);

/*! \ingroup GetMethods
  Get additional information about the tool transformation.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' or 'A' to 'I'

  \return status bits, or zero if there is no information available
  - NDI_BAD_TRANSFORM_FIT   0x01
  - NDI_NOT_ENOUGH_MARKERS  0x02
  - NDI_TOOL_FACE_USED      0x70 - 3 bits give 8 possible faces

  <p>The tool information is only updated when the GX command is called with
  the NDI_ADDITIONAL_INFO (0x0002) mode bit, and then only for the ports
  specified by the NDI_PASSIVE (0x8000) and NDI_PASSIVE_EXTRA (0x2000) bits.
*/
ndicapiExport int ndiGetGXToolInfo(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get additional information about the tool markers.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' or 'A' to 'I'
  \param marker    one of 'A' through 'T' for the 20 markers

  \return status bits, or zero if there is no information available
  - NDI_MARKER_MISSING             0
  - NDI_MARKER_EXCEEDED_MAX_ANGLE  1
  - NDI_MARKER_EXCEEDED_MAX_ERROR  2
  - NDI_MARKER_USED                3

  <p>The tool marker information is only updated when the GX command is
  called with the NDI_ADDITIONAL_INFO (0x0002) mode bit set, and then only
  for the ports specified by the NDI_PASSIVE (0x8000) and
  NDI_PASSIVE_EXTRA (0x2000) bits.
*/
ndicapiExport int ndiGetGXMarkerInfo(ndicapi* pol, int port, int marker);

/*! \ingroup GetMethods
  Get the coordinates of a stray marker on an active tool.
  This command is only meaningful for active tools that have a stray
  marker.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3'
  \param coord     array to hold the three coordinates

  \return the return value will be one of
  - NDI_OKAY - values returned in coord
  - NDI_DISABLED - port disabled or illegal port specified
  - NDI_MISSING - stray marker is not visible to the device

  <p>The stray marker position is only updated when the GX command is
  called with the NDI_SINGLE_STRAY (0x0004) bit set.
*/
ndicapiExport int ndiGetGXSingleStray(ndicapi* pol, int port, double coord[3]);

/*! \ingroup GetMethods
  Get the camera frame number for the latest transform.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' (active ports only)

  \return a 32-bit frame number, or zero if no information was available

  The frame number is only updated when the GX command is called with
  the NDI_FRAME_NUMBER (0x0008) bit, and then only for the ports specified
  by the NDI_PASSIVE (0x8000) and NDI_PASSIVE_EXTRA (0x2000) bits.
*/
ndicapiExport unsigned long ndiGetGXFrame(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get the number of passive stray markers detected.

  \param pol       valid NDI device handle
  \return          a number between 0 and 20

  The passive stray information is updated when a GX command is sent
  with the NDI_PASSIVE_STRAY (0x1000) and NDI_PASSIVE (0x8000) bits set
  in the reply mode.  The information persists until the next time GX is
  sent with these bits set.

  If no information is available, the return value is zero.
*/
ndicapiExport int ndiGetGXNumberOfPassiveStrays(ndicapi* pol);

/*! \ingroup GetMethods
  Copy the coordinates of the specified stray marker into the
  supplied array.

  \param pol       valid NDI device handle
  \param i         a number between 0 and 19
  \param coord     array to hold the coordinates
  \return          one of:
  - NDI_OKAY - information was returned in coord
  - NDI_DISABLED - no stray marker reporting is available
  - NDI_MISSING - marker number i is not visible

  <p>Use ndiGetGXNumberOfPassiveStrays() to get the number of stray
  markers that are visible.

  The passive stray marker coordinates are updated when a GX command
  is sent with the NDI_PASSIVE_STRAY (0x1000) and NDI_PASSIVE (0x8000)
  bits set in the reply mode.  The information persists until the next
  time GX is sent with these bits set.
*/
ndicapiExport int ndiGetGXPassiveStray(ndicapi* pol, int i, double coord[3]);

/*! \ingroup GetMethods
Get the length of the last BX command reply

\param pol       valid NDI device handle

\return the number of bytes in the reply

*/
ndicapiExport unsigned short ndiGetBXReplyLength(ndicapi* pol);

/*! \ingroup GetMethods
Get the transformation for the specified port. The first four numbers are a quaternion,
the next three numbers are the coordinates in millimeters, and the final number is a
unitless error estimate.

\param pol       valid NDI device handle
\param ph        valid port handle in range 0x01 to 0xFF
\param transform space for the 8 numbers in the transformation

\return one of the following:
- NDI_OKAY if successful
- NDI_DISABLED if tool port is nonexistent or disabled
- NDI_MISSING if tool transform cannot be computed

<p>If NDI_DISABLED or NDI_MISSING is returned, then the values in the supplied transform array will be left unchanged.

The transformations for each of the port handles remain the same until the next BX command is sent to device.
*/
ndicapiExport int ndiGetBXTransform(ndicapi* pol, int portHandle, float transform[8]);

/*! \ingroup GetMethods
Get the 16-bit status value for the specified port handle.

\param pol       valid NDI device handle
\param ph        valid port handle in range 0x01 to 0xFF

\return status bits or zero if there is no information:
- NDI_TOOL_IN_PORT        0x0001
- NDI_SWITCH_1_ON         0x0002
- NDI_SWITCH_2_ON         0x0004
- NDI_SWITCH_3_ON         0x0008
- NDI_INITIALIZED         0x0010
- NDI_ENABLED             0x0020
- NDI_OUT_OF_VOLUME       0x0040
- NDI_PARTIALLY_IN_VOLUME 0x0080

This information is updated each time that the BX command
is sent to the device.
*/
ndicapiExport int ndiGetBXPortStatus(ndicapi* pol, int portHandle);

/*! \ingroup GetMethods
Get the camera frame number for the latest transform.

\param pol       valid NDI device handle
\param ph        valid port handle in range 0x01 to 0xFF

\return a 32-bit frame number, or zero if no information was available

This information is updated each time that the BX command
is sent to the device.
*/
ndicapiExport unsigned long ndiGetBXFrame(ndicapi* pol, int portHandle);

/*! \ingroup GetMethods
Get additional information about the tool transformation.

\param pol          valid NDI device handle
\param ph           valid port handle in range 0x01 to 0xFF
\param outToolInfo  parameter to hold tool information

\return the return value will be one of
- NDI_OKAY - values returned in coord
- NDI_DISABLED - port disabled or illegal port specified
- NDI_MISSING - stray marker is not visible to the device

<p>The tool information is only updated when the BX command is called with
the NDI_ADDITIONAL_INFO (0x0002) mode bit.
*/
ndicapiExport int ndiGetBXToolInfo(ndicapi* pol, int portHandle, char& outToolInfo);

/*! \ingroup GetMethods
Get additional information about the tool markers.

\param pol            valid NDI device handle
\param ph             valid port handle in range 0x01 to 0xFF
\param marker         one of 'A' through 'T' for the 20 markers
\param outMarkerInfo status bits, or zero if there is no information available
- NDI_MARKER_MISSING             0
- NDI_MARKER_EXCEEDED_MAX_ANGLE  1
- NDI_MARKER_EXCEEDED_MAX_ERROR  2
- NDI_MARKER_USED                3

\return the return value will be one of
- NDI_OKAY - values returned in coord
- NDI_DISABLED - port disabled or illegal port specified
- NDI_MISSING - stray marker is not visible to the device

<p>The tool marker information is only updated when the BX command is
called with the NDI_ADDITIONAL_INFO (0x0002) mode bit set.
*/
ndicapiExport int ndiGetBXMarkerInfo(ndicapi* pol, int portHandle, int marker, char& outMarkerInfo);

/*! \ingroup GetMethods
Get the coordinates of a stray marker on a wired POLARIS tool.
This command is only meaningful for tools that have a stray
marker.

\param pol       valid NDI device handle
\param ph        valid port handle in range 0x01 to 0xFF
\param coord     array to hold the three coordinates

\return the return value will be one of
- NDI_OKAY - values returned in coord
- NDI_DISABLED - port disabled or illegal port specified
- NDI_MISSING - stray marker is not visible to the device

<p>The stray marker position is only updated when the BX command is
called with the NDI_SINGLE_STRAY (0x0004) bit set.
*/
ndicapiExport int ndiGetBXSingleStray(ndicapi* pol, int portHandle, float coord[3]);

/*! \ingroup GetMethods
Get the number of passive stray markers detected.

\param pol       valid NDI device handle
\return          a number between 0 and 20

The passive stray marker coordinates are updated when a BX command
is sent with the NDI_PASSIVE_STRAY (0x1000) bit set in the reply mode.
*/
ndicapiExport int ndiGetBXNumberOfPassiveStrays(ndicapi* pol);

/*! \ingroup GetMethods
Copy the coordinates of the specified stray marker into the
supplied array.

\param pol       valid NDI device handle
\param i         a number between 0 and 19
\param coord     array to hold the coordinates
\return          one of:
- NDI_OKAY - information was returned in coord
- NDI_DISABLED - no stray marker reporting is available
- NDI_MISSING - marker number i is not visible

<p>Use ndiGetTXNumberOfPassiveStrays() to get the number of stray
markers that are visible.

The passive stray marker coordinates are updated when a BX command
is sent with the NDI_PASSIVE_STRAY (0x1000) bit set in the reply mode.
*/
ndicapiExport int ndiGetBXPassiveStray(ndicapi* pol, int i, float coord[3]);

/*! \ingroup GetMethods
Get an 16-bit status bitfield for the system.

\param pol       valid NDI device handle

\return status bits or zero if there is no information:
- NDI_COMM_SYNC_ERROR            0x0001
- NDI_TOO_MUCH_EXTERNAL_INFRARED 0x0002
- NDI_COMM_CRC_ERROR             0x0004
- NDI_COMM_RECOVERABLE           0x0008
- NDI_HARDWARE_FAILURE           0x0010
- NDI_HARDWARE_CHANGE            0x0020
- NDI_PORT_OCCUPIED              0x0040
- NDI_PORT_UNOCCUPIED            0x0080

<p>The system status information is updated whenever the BX command is
called with the NDI_XFORMS_AND_STATUS (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetBXSystemStatus(ndicapi* pol);

/*! \ingroup GetMethods
Get the transformation for the specified port. The first four numbers are a quaternion,
the next three numbers are the coordinates in millimeters, and the final number is a
unitless error estimate.

\param pol       valid NDI device handle
\param ph        valid port handle in range 0x01 to 0xFF
\param transform space for the 8 numbers in the transformation

\return one of the following:
- NDI_OKAY if successful
- NDI_DISABLED if tool port is nonexistent or disabled
- NDI_MISSING if tool transform cannot be computed

<p>If NDI_DISABLED or NDI_MISSING is returned, then the values in the supplied transform array will be left unchanged.

The transformations for each of the port handles remain the same until the next BX2 command is sent to device.
*/
ndicapiExport int ndiGetBX2Transform(ndicapi* pol, int portHandle, float transform[8]);

/*! \ingroup GetMethods
Get the 16-bit status value for the specified port handle.

\param pol         valid NDI device handle
\param portHandle  valid port handle in range 0x01 to 0xFF

This information is updated each time that the BX2 command is sent to the device.
*/
ndicapiExport unsigned short ndiGetBX2PortStatus(ndicapi* pol, int portHandle);

/*! \ingroup GetMethods
Get the length of the last BX2 command reply

\param pol       valid NDI device handle
\param portHandle  valid port handle in range 0x01 to 0xFF

\return the number of bytes in the reply

*/
ndicapiExport unsigned int ndiGetBX2ReplyLength(ndicapi* pol);

/*! \ingroup GetMethods
Get an 16-bit status bitfield for the system.

\param pol       valid NDI device handle
\param index     the alert to access, use ndiGetBX2SystemAlertsCount

<p>The system status information is updated whenever the BX2 command is called
*/
ndicapiExport unsigned short* ndiGetBX2SystemAlert(ndicapi* pol, int index);
ndicapiExport int ndiGetBX2SystemAlertsCount(ndicapi* pol);

/*! \ingroup GetMethods
Get the camera frame number for the latest BX2 frame.

\param pol       valid NDI device handle

\return a 32 - bit frame number, or zero if no information was available

This information is updated each time that the BX2 command is sent to the device.
*/
ndicapiExport unsigned int ndiGetBX2Frame(ndicapi * pol);

/*! \ingroup GetMethods
Get the camera frame number for the latest BX2 frame.

\param pol         valid NDI device handle
\param portHandle  valid port handle in range 0x01 to 0xFF

\return a boolean indicating if averaging is enabled for the given handle

This information is updated each time that the BX2 command is sent to the device.
*/
ndicapiExport bool ndiGetBX2HandleAveragingEnabled(ndicapi* pol, int portHandle);


/*! \ingroup GetMethods
  Get the 8-bit status value for the specified port.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' or 'A' to 'I'

  \return a an integer composed of the following bits:
  - NDI_TOOL_IN_PORT        0x01 - there is a tool in the port
  - NDI_SWITCH_1_ON         0x02 - button 1 is pressed
  - NDI_SWITCH_2_ON         0x04 - button 2 is pressed
  - NDI_SWITCH_3_ON         0x08 - button 3 is pressed
  - NDI_INITIALIZED         0x10 - tool port has been initialized
  - NDI_ENABLED             0x20 - tool port is enabled for tracking
  - NDI_CURRENT_DETECT      0x80 - tool sensed through current detection

  <p>If there is no available information for the specified port, or if
  an illegal port specifier value is used, the return value is zero.
  The return value is updated only when a PSTAT command is sent with
  the NDI_BASIC (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetPSTATPortStatus(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get a 30-byte string describing the tool in the specified port.

  \param pol         valid NDI device handle
  \param port        one of '1', '2', '3' or 'A' to 'I'
  \param information array that information is returned in (the
                     resulting string is not null-terminated)

  \return  one of:
  - NDI_OKAY - information was returned
  - NDI_UNOCCUPIED - port is unoccupied or no information is available

  <p>The returned string will not be null-terminated by default.  You
  must set information[30] to 0 in order to terminate the string.
  If the port is unoccupied then the contents of the \em information
  string are undefined.

  The information is updated only when a PSTAT command is sent with
  the NDI_BASIC (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetPSTATToolInfo(ndicapi* pol, int port, char information[30]);

/*! \ingroup GetMethods
  Return the results of a current test on the IREDS on the specified
  tool.

  \param  ts          valid NDI device handle
  \param  port        one of '1', '2', '3'

  \return 32-bit integer (see NDI documentation)

  The information is updated only when a PSTAT command is sent with
  the NDI_TESTING (0x0002) bit set in the reply mode.
*/
ndicapiExport unsigned long ndiGetPSTATCurrentTest(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get a 20-byte string that contains the part number of the tool.

  \param pol         valid NDI device handle
  \param port        one of '1', '2', '3' or 'A' to 'I'
  \param part        array that part number is returned in (the
                     resulting string is not null-terminated)

  \return  one of:
  - NDI_OKAY - information was returned
  - NDI_UNOCCUPIED - port is unoccupied or no information is available

  <p>If a terminated string is required, then set part[20] to '\0'
  before calling this function.

  The information is updated only when a PSTAT command is sent with
  the NDI_PART_NUMBER (0x0004) bit set in the reply mode.
*/
ndicapiExport int ndiGetPSTATPartNumber(ndicapi* pol, int port, char part[20]);

/*! \ingroup GetMethods
  Get the 8-bit value specifying the accessories for the specified tool.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' (active ports only)

  \return a an integer composed of the following bits:
  - NDI_TOOL_IN_PORT_SWITCH   0x01  - tool has tool-in-port switch
  - NDI_SWITCH_1              0x02  - tool has button 1
  - NDI_SWITCH_2              0x04  - tool has button 2
  - NDI_SWITCH_3              0x08  - tool has button 3
  - NDI_TOOL_TRACKING_LED     0x10  - tool has tracking LED
  - NDI_LED_1                 0x20  - tool has LED 1
  - NDI_LED_2                 0x40  - tool has LED 2
  - NDI_LED_3                 0x80  - tool has LED 3

  <p>If there is no available information for the specified port, or if
  an illegal port specifier value is used, the return value is zero.
  The return value is always zero for passive ports.

  The return value is updated only when a PSTAT command is sent with
  the NDI_ACCESSORIES (0x0008) bit set in the reply mode.
*/
ndicapiExport int ndiGetPSTATAccessories(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get an 8-bit value describing the marker type for the tool.
  The low three bits descibe the wavelength, and the high three
  bits are the marker type code.

  \param pol       valid NDI device handle
  \param port      one of '1', '2', '3' (active ports only)

  \return  see NDI documentation for more information:
  - low bits:
    - NDI_950NM            0x00
    - NDI_850NM            0x01
  - high bits:
    - NDI_NDI_ACTIVE       0x08
    - NDI_NDI_CERAMIC      0x10
    - NDI_PASSIVE_ANY      0x20
    - NDI_PASSIVE_SPHERE   0x28
    - NDI_PASSIVE_DISC     0x30

  <p>If there is no available information for the specified port, or if
  an illegal port specifier value is used, the return value is zero.

  The return value is updated only when a PSTAT command is sent with
  the NDI_MARKER_TYPE (0x0010) bit set in the reply mode.
*/
ndicapiExport int ndiGetPSTATMarkerType(ndicapi* pol, int port);

/*! \ingroup GetMethods
  Get the status of the control processor.

  \param pol       valid NDI device handle

  \return an int with the following bit definitions for errors:
  - NDI_EPROM_CODE_CHECKSUM     0x01
  - NDI_EPROM_SYSTEM_CHECKSUM   0x02

  <p>This information is updated only when the SSTAT command is sent
  with the NDI_CONTROL (0x0001) bit set in the reply mode.
*/
ndicapiExport int ndiGetSSTATControl(ndicapi* pol);

/*! \ingroup GetMethods
  Get the status of the sensor processors.

  \param pol       valid NDI device handle

  \return an int with the following bit definitions for errors:
  - NDI_LEFT_ROM_CHECKSUM   0x01
  - NDI_LEFT_SYNC_TYPE_1    0x02
  - NDI_LEFT_SYNC_TYPE_2    0x04
  - NDI_RIGHT_ROM_CHECKSUM  0x10
  - NDI_RIGHT_SYNC_TYPE_1   0x20
  - NDI_RIGHT_SYNC_TYPE_2   0x40

  <p>This information is updated only when the SSTAT command is sent
  with the NDI_SENSORS (0x0002) bit set in the reply mode.
*/
ndicapiExport int ndiGetSSTATSensors(ndicapi* pol);

/*! \ingroup GetMethods
  Get the status of the sensor processors.

  \param pol       valid NDI device handle

  \return an int with the following bit definitions for errors:
  - NDI_ROM_CHECKSUM        0x01
  - NDI_OPERATING_VOLTAGES  0x02
  - NDI_MARKER_SEQUENCING   0x04
  - NDI_SYNC                0x08
  - NDI_COOLING_FAN         0x10
  - NDI_INTERNAL_ERROR      0x20

  <p>This information is updated only when the SSTAT command is sent
  with the NDI_TIU (0x0004) bit set in the reply mode.
*/
ndicapiExport int ndiGetSSTATTIU(ndicapi* pol);

/*! \ingroup GetMethods
  Check to see whether environmental infrared was detected.

  \param pol    valid NDI device handle

  \return       1 if infrared was detected and 0 otherwise.

  This information is only updated if the IRCHK command is called
  with the NDI_DETECTED (0x0001) format bit set.
*/
ndicapiExport int ndiGetIRCHKDetected(ndicapi* pol);

/*! \ingroup GetMethods
  Get the number of infrared sources seen by one of the two sensors.

  \param pol    valid NDI device handle
  \param side   one of NDI_LEFT or NDI_RIGHT

  return  the number of infrared sources seen by the specified sensor

  This information is valid only immediately after the IRCHK command
  has been called with the NDI_SOURCES (0x0002) format bit set.  Otherwise,
  the return value will be zero.
*/
ndicapiExport int ndiGetIRCHKNumberOfSources(ndicapi* pol, int side);

/*! \ingroup GetMethods
  Get the coordinates of one of the infrared sources seen by one of
  the two sensors.

  \param pol    valid NDI device handle
  \param side   one of NDI_LEFT or NDI_RIGHT
  \param xy     space to store the returned coordinates

  return  NDI_OKAY or NDI_MISSING

  If there is no available information, then the xy array will be left
  unchanged and the value NDI_MISSING will be returned.  Otherwise,
  the return value will be NDI_OKAY.

  This information is valid only immediately after the IRCHK command
  has been called with the NDI_SOURCES (0x0002) format bit set.
*/
ndicapiExport int ndiGetIRCHKSourceXY(ndicapi* pol, int side, int i, double xy[2]);

/*=====================================================================*/
/*! \defgroup ConversionFunctions Conversion Functions
  These are a set of helper functions for doing miscellaneous conversions
  to and from ascii.
*/

/*! \ingroup ConversionFunctions
  Convert an error code returned by ndiGetError() into a string that
  describes the error.  The error codes are listed in \ref ErrorCodes.

  An unrecognized error code will return "Unrecognized error code".
*/
ndicapiExport const char* ndiErrorString(int errnum);

/*! \ingroup ConversionFunctions
  Convert \em n characters of a hexidecimal string into an unsigned long.
  The conversion halts if a non-hexidecimal digit is found.

  The primary use of this function is decoding replies from the device.
*/
ndicapiExport unsigned long ndiHexToUnsignedLong(const char* cp, int n);

/*! \ingroup ConversionFunctions
Convert \em n characters of a hexidecimal string into an unsigned int.
The conversion halts if a non-hexidecimal digit is found.

The primary use of this function is decoding replies from the device.
*/
ndicapiExport unsigned int ndiHexToUnsignedInt(const char* cp, int n);

/*! \ingroup ConversionFunctions
  Convert \em n characters of a signed decimal string to a long.
  The first character in the string must be '+' or '-', otherwise
  the result will be zero.

  The primary use of this function is decoding replies from the
  device.
*/
ndicapiExport long ndiSignedToLong(const char* cp, int n);

/*! \ingroup ConversionFunctions
  This function is used to convert raw binary data into a stream of
  hexidecimal digits that can be sent to the device.
  The length of the output string will be twice the number of bytes
  in the input data, since each byte will be represented by two
  hexidecimal digits.

  As a convenience, the return value is a pointer to the hexidecimal
  string.  If the string must be terminated, then set cp[2*n] to '\0'
  before calling this function, otherwise the string will be left
  unterminated.
*/
ndicapiExport char* ndiHexEncode(char* cp, const void* data, int n);

/*! \ingroup ConversionFunctions
  This function converts a hex-encoded string into binary data.
  This can be used to decode the SROM data sent from the device.
  The length of the input string must be twice the expected number
  of bytes in the output data, since each binary byte is ecoded by
  two hexidecimal digits.

  As a convenience, the return value is a pointer to the decoded data.
*/
ndicapiExport void* ndiHexDecode(void* data, const char* cp, int n);

/*=====================================================================*/
/*! \defgroup ErrorCodes Error Codes
  The error code is set only by ndiCommand() or by
  macros and functions that call ndiCommand().

  Error codes that equal to or less than 0xff are error codes reported
  by the device itself.  Error codes greater than 0xff are
  errors that are reported by the host computer.

  The error code is returned by ndiGetError() and the corresponding
  text is available by passing the code to ndiErrorString().
*/

/*\{*/
#define NDI_OKAY                   0x00  /*!<\brief No error */
#define NDI_INVALID                0x01  /*!<\brief Invalid command */
#define NDI_TOO_LONG               0x02  /*!<\brief Command too long */
#define NDI_TOO_SHORT              0x03  /*!<\brief Command too short */
#define NDI_BAD_COMMAND_CRC        0x04  /*!<\brief Bad CRC calculated for command */
#define NDI_INTERN_TIMEOUT         0x05  /*!<\brief Timeout on command execution */
#define NDI_COMM_FAIL              0x06  /*!<\brief New communication parameters failed */
#define NDI_PARAMETERS             0x07  /*!<\brief Incorrect number of command parameters */
#define NDI_INVALID_PORT           0x08  /*!<\brief Invalid port selected */
#define NDI_INVALID_MODE           0x09  /*!<\brief Invalid mode selected */
#define NDI_INVALID_LED            0x0a  /*!<\brief Invalid LED selected */
#define NDI_LED_STATE              0x0b  /*!<\brief Invalid LED state selected */
#define NDI_BAD_MODE               0x0c  /*!<\brief Command invalid for current mode */
#define NDI_NO_TOOL                0x0d  /*!<\brief No tool plugged in selected port */
#define NDI_PORT_NOT_INIT          0x0e  /*!<\brief Selected port not initialized */
#define NDI_PORT_DISABLED          0x0f  /*!<\brief Selected port not enabled */
#define NDI_INITIALIZATION         0x10  /*!<\brief System not initialized */
#define NDI_TSTOP_FAIL             0x11  /*!<\brief Failure to stop tracking */
#define NDI_TSTART_FAIL            0x12  /*!<\brief Failure to start tracking */
#define NDI_PINIT_FAIL             0x13  /*!<\brief Failure to initialize tool in port */
#define NDI_INVALID_PS_PARAM       0x14  /*!<\brief Invalid Position Sensor characterization parameters */
#define NDI_INIT_FAIL              0x15  /*!<\brief Unable to initialize the system */
#define NDI_DSTART_FAIL            0x16  /*!<\brief Failure to start diagnostic mode */
#define NDI_DSTOP_FAIL             0x17  /*!<\brief Failure to stop diagnostic mode */
#define NDI_RESERVED18             0x18  /*!<\brief Reserved 0x18 */
#define NDI_FIRMWARE_VER           0x19  /*!<\brief Failure to read firmware version */
#define NDI_INTERNAL               0x1a  /*!<\brief Internal device error */
#define NDI_RESERVED1B             0x1b  /*!<\brief Reserved 0x1B */
#define NDI_IRED_FAIL              0x1c  /*!<\brief Failure to set marker firing signature */
#define NDI_RESERVED1D             0x1d  /*!<\brief Reserved 0x1D */
#define NDI_SROM_READ              0x1e  /*!<\brief Failure to read SROM data */
#define NDI_SROM_WRITE             0x1f  /*!<\brief Failure to write SROM data */
#define NDI_RESERVED20             0x20  /*!<\brief Reserved 0x20 */
#define NDI_PORT_CURRENT           0x21  /*!<\brief Failure to perform tool current test */
#define NDI_WAVELENGTH             0x22  /*!<\brief Enabled tools are not supported by selected volume parameters. For example, a Position Sensor cannot track a tool if the volume parameter set does not include the marker wavelength of an enabled tool. */
#define NDI_PARAMETER_RANGE        0x23  /*!<\brief Command parameter out of range */
#define NDI_VOLUME                 0x24  /*!<\brief No camera parameters for this volume */
#define NDI_FEATURES               0x25  /*!<\brief Failure to determine supported features*/
#define NDI_RESERVED26             0x26  /*!<\brief Reserved 0x26 */
#define NDI_RESERVED27             0x27  /*!<\brief Reserved 0x27 */
#define NDI_TOO_MANY_TOOLS         0x28 /*!<\brief Too many tools are enabled, the configuration of tools loaded requires too many frames, or this tool type is not supported. */
#define NDI_RESERVED29             0x29 /*!<\brief Reserved 0x29 */
#define NDI_HEAP_FULL              0x2A /*!<\brief No memory is available for dynamic allocation(heap is full). */
#define NDI_HANDLE_NOT_ALLOC       0x2B /*!<\brief The requested port handle has not been allocated. */
#define NDI_HANDLE_EMPTY           0x2C /*!<\brief The requested port handle has become unoccupied. */
#define NDI_HANDLES_FULL           0x2D /*!<\brief All handles have been allocated. */
#define NDI_INCOMP_FIRM_VER        0x2E /*!<\brief Incompatible firmware versions.This can occur if: � a firmware update failed � components with incompatible firmware are connected To correct the problem, update the firmware.If the Multi Firmware feature is installed, select a valid combined firmware revision.For more information, see �Multi Firmware Feature� on page 197. */
#define NDI_INV_PORT_DESC          0x2F /*!<\brief Invalid port description. */
#define NDI_PORT_HAS_HANDLE        0x30 /*!<\brief Requested port is already assigned a port handle. */
#define NDI_RESERVED31             0x31 /*!<\brief Reserved 0x31 */
#define NDI_INVALID_OP             0x32 /*!<\brief Invalid operation for the device associated with the specified port handle. */
#define NDI_NO_FEATURE             0x33 /*!<\brief Feature not available. */
#define NDI_NO_USER_PARAM          0x34 /*!<\brief User parameter does not exist. */
#define NDI_BAD_VALUE              0x35 /*!<\brief Invalid value type(e.g.string instead of integer). */
#define NDI_USER_PARAM_VALUE_RANGE 0x36 /*!<\brief User parameter value set is out of valid range. */
#define NDI_USER_PARAM_INDEX_RANGE 0x37 /*!<\brief User parameter array index is out of valid range. */
#define NDI_BAD_USER_PARAM_SIZE    0x38 /*!<\brief User parameter size is incorrect. */
#define NDI_PERM_DENIED            0x39 /*!<\brief Permission denied; file or user parameter is read - only, or a command which requires master mode is attempted from a monitor mode connection. */
#define NDI_RESERVED3A             0x3A /*!<\brief Reserved 0x3A */
#define NDI_FILE_NOT_FOUND         0x3B /*!<\brief File not found. */
#define NDI_FERR_WRITE             0x3C /*!<\brief Error writing to file. */
#define NDI_FERR_READ              0x3D /*!<\brief Error reading from file. */
#define NDI_RESERVED3E             0x3E /*!<\brief Reserved 0x3E */
#define NDI_RESERVED3F             0x3F /*!<\brief Reserved 0x3F*/
#define NDI_DEF_FILE_ERR           0x40 /*!<\brief Tool Definition File Error.This occurs if: � the CRC failed � the file format is invalid */
#define NDI_BAD_CHARACTERISTICS    0x41 /*!<\brief Tool characteristics not supported.This occurs when one of the following fields in the tool definition file is outside of the range supported by the system : � number of markers � number of faces � number of groups � number of markers per face(unique geometry tools only) */
#define NDI_NO_DEVICE              0x42 /*!<\brief Device not present.This occurs when the command is specific to a device that is not connected to the system. */

#define NDI_ENVIRONMENT     0xf1  /*!<\brief Too much environmental infrared */
#define NDI_EEPROM_ERASE    0xf4  /*!<\brief Failure to erase Flash EEPROM */
#define NDI_EEPROM_WRITE    0xf5  /*!<\brief Failure to write Flash EEPROM */
#define NDI_EEPROM_READ     0xf6  /*!<\brief Failure to read Flash EEPROM */

/* error codes returned by the C api */

#define NDI_BAD_CRC         0x0100  /*!<\brief Bad CRC received from device */
#define NDI_OPEN_ERROR      0x0101  /*!<\brief Error opening serial device */
#define NDI_BAD_COMM        0x0102  /*!<\brief Bad communication parameters for host*/
#define NDI_TIMEOUT         0x0103  /*!<\brief Device took >5 secs to reply */
#define NDI_WRITE_ERROR     0x0104  /*!<\brief Device write error */
#define NDI_READ_ERROR      0x0105  /*!<\brief Device read error */
#define NDI_RESET_FAIL      0x0106  /*!<\brief Device failed to reset on break */
#define NDI_PROBE_FAIL      0x0107  /*!<\brief Device not found on specified port */

#define NDI_DSR_FAILURE           0x0200  /*!<\brief Bad DSR query failure */
#define NDI_BAD_REPLY             0x0201  /*!<\brief Bad reply from measurement system */
#define NDI_NO_FEATURES_FIRMWARE  0x0202  /*!<\brief System doesn't support Features.Firmware */
#define NDI_COMMAND_VER_FAILED    0x0203  /*!<\brief VER command failed */
/*\}*/

/* ndi GBF component IDs */
/*\{*/
#define NDI_COMPONENTID_FRAME      0x0001
#define NDI_COMPONENTID_6D         0x0002
#define NDI_COMPONENTID_3D         0x0003
#define NDI_COMPONENTID_1D         0x0004
#define NDI_COMPONENTID_2D         0x0005
// 06 � reserved
// 07 � reserved
#define NDI_COMPONENTID_LINE_SEP   0x0008
#define NDI_COMPONENTID_3D_ERROR   0x0009
#define NDI_COMPONENTID_IMAGE      0x0010
// 11 to 16 � reserved
#define NDI_COMPONENTID_UV         0x0011
#define NDI_COMPONENTID_SYS_ALERT  0x0012
/*\}*/

/* ndiCOMM() baud rates */
/*\{*/
#define  NDI_9600     0
#define  NDI_14400    1
#define  NDI_19200    2
#define  NDI_38400    3
#define  NDI_57600    4
#define  NDI_115200   5
#define  NDI_921600   6
#define  NDI_1228739  7
#define  NDI_230400   10
/*\}*/

/* ndiCOMM() data bits, parity and stop bits */
/*\{*/
#define  NDI_8N1      0              /* 8 data bits, no parity, 1 stop bit */
#define  NDI_8N2      1              /* etc. */
#define  NDI_8O1     10
#define  NDI_8O2     11
#define  NDI_8E1     20
#define  NDI_8E2     21
#define  NDI_7N1    100
#define  NDI_7N2    101
#define  NDI_7O1    110
#define  NDI_7O2    111
#define  NDI_7E1    120
#define  NDI_7E2    121
/*\}*/

/* ndiCOMM() hardware handshaking */
/*\{*/
#define  NDI_NOHANDSHAKE  0
#define  NDI_HANDSHAKE    1
/*\}*/

/* PHSR() handle types */
/*\{*/
#define  NDI_ALL_HANDLES            0x00
#define  NDI_STALE_HANDLES          0x01
#define  NDI_UNINITIALIZED_HANDLES  0x02
#define  NDI_UNENABLED_HANDLES      0x03
#define  NDI_ENABLED_HANDLES        0x04
/*\}*/

/* ndiPENA() tracking modes */
/*\{*/
#define  NDI_STATIC      'S'    /* relatively immobile tool */
#define  NDI_DYNAMIC     'D'    /* dynamic tool (e.g. probe) */
#define  NDI_BUTTON_BOX  'B'    /* tool with no IREDs */
/*\}*/

/* ndiGX() reply mode bit definitions */
/*\{*/
#define  NDI_XFORMS_AND_STATUS  0x0001  /* transforms and status */
#define  NDI_ADDITIONAL_INFO    0x0002  /* additional tool transform info */
#define  NDI_SINGLE_STRAY       0x0004  /* stray active marker reporting */
#define  NDI_FRAME_NUMBER       0x0008  /* frame number for each tool */
#define  NDI_PASSIVE            0x8000  /* report passive tool information */
#define  NDI_PASSIVE_EXTRA      0x2000  /* add 6 extra passive tools */
#define  NDI_PASSIVE_STRAY      0x1000  /* stray passive marker reporting */
/*\}*/

/* ndiBX() reply mode bit definitions */
/*\{*/
#define  NDI_3D_MARKER_POSITIONS        0x0008  /* 3d positions of markers on tools */
#define  NDI_NOT_NORMALLY_REPORTED      0x0800  /* report all transformations */
#define  NDI_ACTIVE_STRAY_VALID         0x01
#define  NDI_ACTIVE_STRAY_MISSING       0x02
#define  NDI_ACTIVE_STRAY_OUT_OF_VOLUME 0x08
/*\}*/

/* return values that give the reason behind missing data */
/*\{*/
#define NDI_DISABLED        1
#define NDI_MISSING         2
#define NDI_UNOCCUPIED      3
/*\}*/

/* return values for handle status */
#define NDI_HANDLE_VALID          0x01
#define NDI_HANDLE_MISSING        0x02
#define NDI_HANDLE_DISABLED       0x04

/* ndiGetTXPortStatus() and ndiGetPSTATPortStatus() return value bits */
/*\{*/
#define  NDI_TOOL_IN_PORT         0x01
#define  NDI_SWITCH_1_ON          0x02
#define  NDI_SWITCH_2_ON          0x04
#define  NDI_SWITCH_3_ON          0x08
#define  NDI_INITIALIZED          0x10
#define  NDI_ENABLED              0x20
#define  NDI_OUT_OF_VOLUME        0x40 /* only for ndiGetGXPortStatus() */
#define  NDI_PARTIALLY_IN_VOLUME  0x80 /* only for ndiGetGXPortStatus() */
#define  NDI_CURRENT_DETECT       0x80 /* only for ndiGetPSTATPortStatus() */
/*\}*/

/* ndiGetBX2PortStatus() return value bits */
/*\{*/
#define NDI_BX2_ENABLED               0 //Enabled
#define NDI_BX2_PARTIAL_VIEW          3 //Tool is partially out of the characterized measurement volume
#define NDI_BX2_PARTIAL_AVG           5 //Data is partially averaged; averaging depth is less than specified
#define NDI_BX2_OUT_OF_VOLUME         9 //Tool is out of the characterized measurement volume

// The following error codes are reported only if the tool is missing
#define NDI_BX2_TOO_FEW_MARKERS       13 // Too few markers detected
#define NDI_BX2_IR_INTERFERENCE       14 // IR interference (a large bright IR object)
#define NDI_BX2_BAD_XFORM_FIT         17 // Bad transformation fit
#define NDI_BX2_BUFFER_LIMIT          18 // Data buffer limitation (too much data; for example, too many markers)
#define NDI_BX2_ALGO_LIMIT            19 // Algorithm limitation (processing requires more buffer than is available)
#define NDI_BX2_CANT_KEEP_UP          20 // Fell behind while processing
#define NDI_BX2_OUT_OF_SYNC           21 // Position sensors out of synch
#define NDI_BX2_PROC_EXCEP            22 // Processing exception
#define NDI_BX2_TOOL_MISSING          31 // Tool is missing
#define NDI_BX2_TRACKING_NOT_ENABLED  32 // Tracking is not enabled for this tool
#define NDI_BX2_TOOL_UNPLUGGED        33 // Tool has been unplugged from the System Control Unit
/*\}*/

#define NDI_BX2_MISSING_BIT 0x0100
#define NDI_BX2_AVG_BIT     0x0200

#define NDI_SYS_ALERT_FAULT 0x01
#define NDI_SYS_ALERT_ALERT 0x02
#define NDI_SYS_ALERT_EVENT 0x04

#define NDI_SYS_EVENT_TOOL_CONNECTED      1
#define NDI_SYS_EVENT_TOOL_DISCONNECTED   2
#define NDI_SYS_EVENT_HW_CHANGED          5
#define NDI_SYS_EVENT_PTP_MASTER_CHANGED  6

/* ndiGetTXSystemStatus() return value bits */
/*\{*/
#define  NDI_COMM_SYNC_ERROR            0x0001
#define  NDI_TOO_MUCH_EXTERNAL_INFRARED 0x0002
#define  NDI_COMM_CRC_ERROR             0x0004
#define  NDI_COMM_RECOVERABLE           0x0008
#define  NDI_HARDWARE_FAILURE           0x0010
#define  NDI_HARDWARE_CHANGE            0x0020
#define  NDI_PORT_OCCUPIED              0x0040
#define  NDI_PORT_UNOCCUPIED            0x0080
/*\}*/

/* ndiGetGXToolInfo() return value bits */
/*\{*/
#define  NDI_BAD_TRANSFORM_FIT   0x01
#define  NDI_NOT_ENOUGH_MARKERS  0x02
#define  NDI_TOOL_FACE_USED      0x70
/*\}*/

/* ndiGetTXMarkerInfo() return value bits */
/*\{*/
#define  NDI_MARKER_MISSING             0
#define  NDI_MARKER_EXCEEDED_MAX_ANGLE  1
#define  NDI_MARKER_EXCEEDED_MAX_ERROR  2
#define  NDI_MARKER_USED                3
/*\}*/

/* ndiLED() states */
/*\{*/
#define  NDI_BLANK 'B'    /* off */
#define  NDI_FLASH 'F'    /* flashing */
#define  NDI_SOLID 'S'    /* on */
/*\}*/

/* ndiPSTAT() reply mode bits */
/*\{*/
#define  NDI_BASIC           0x0001
#define  NDI_TESTING         0x0002
#define  NDI_PART_NUMBER     0x0004
#define  NDI_ACCESSORIES     0x0008
#define  NDI_MARKER_TYPE     0x0010
#define  NDI_PORT_LOCATION   0x0020
#define  NDI_GPIO_STATUS     0x0040
/*\}*/

/* ndiGetPSTATAccessories() return value bits */
/*\{*/
#define  NDI_TOOL_IN_PORT_SWITCH   0x01  /* tool has tool-in-port switch */
#define  NDI_SWITCH_1              0x02  /* tool has button #1 */
#define  NDI_SWITCH_2              0x04  /* tool has button #2 */
#define  NDI_SWITCH_3              0x08  /* tool has button #3 */
#define  NDI_TOOL_TRACKING_LED     0x10  /* tool has tracking LED */
#define  NDI_LED_1                 0x20  /* tool has LED #1 */
#define  NDI_LED_2                 0x40  /* tool has LED #2 */
#define  NDI_LED_3                 0x80  /* tool has LED #3 */
/*\}*/

/* ndiGetPSTATMarkerType() return value, the first 3 bits */
/*\{*/
#define NDI_950NM            0x00
#define NDI_850NM            0x01
/*\}*/

/* ndiGetPSTATMarkerType() return value, the last 5 bits */
/*\{*/
#define NDI_NDI_ACTIVE       0x08
#define NDI_NDI_CERAMIC      0x10
#define NDI_PASSIVE_ANY      0x20
#define NDI_PASSIVE_SPHERE   0x28
#define NDI_PASSIVE_DISC     0x30
/*\}*/

/* tool type specifiers */
/*\{*/
#define NDI_TYPE_REFERENCE   0x01
#define NDI_TYPE_POINTER     0x02
#define NDI_TYPE_BUTTON      0x03
#define NDI_TYPE_SOFTWARE    0x04
#define NDI_TYPE_MICROSCOPE  0x05
#define NDI_TYPE_DOCK        0x08
#define NDI_TYPE_CARM        0x0A
#define NDI_TYPE_CATHETER    0x0B
/*\}*/

/* ndiSSTAT() reply format bits */
/*\{*/
#define NDI_CONTROL         0x0001  /* control processor information */
#define NDI_SENSORS         0x0002  /* sensor processors */
#define NDI_TIU             0x0004  /* TIU processor */
/*\}*/

/* ndiGetSSTATControl() return value bits */
/*\{*/
#define NDI_EPROM_CODE_CHECKSUM     0x01
#define NDI_EPROM_SYSTEM_CHECKSUM   0x02
/*\}*/

/* ndiGetSSTATSensor() return value bits */
/*\{*/
#define NDI_LEFT_ROM_CHECKSUM   0x01
#define NDI_LEFT_SYNC_TYPE_1    0x02
#define NDI_LEFT_SYNC_TYPE_2    0x04
#define NDI_RIGHT_ROM_CHECKSUM  0x10
#define NDI_RIGHT_SYNC_TYPE_1   0x20
#define NDI_RIGHT_SYNC_TYPE_2   0x40
/*\}*/

/* ndiGetSSTATTIU() return value bits */
/*\{*/
#define NDI_ROM_CHECKSUM        0x01
#define NDI_OPERATING_VOLTAGES  0x02
#define NDI_MARKER_SEQUENCING   0x04
#define NDI_SYNC                0x08
#define NDI_COOLING_FAN         0x10
#define NDI_INTERNAL_ERROR      0x20
/*\}*/

/* ndiIRCHK() reply mode bits */
/*\{*/
#define  NDI_DETECTED   0x0001   /* simple yes/no whether IR detected */
#define  NDI_SOURCES    0x0002   /* locations of up to 20 sources per camera */
/*\}*/

/* ndiGetIRCHKNumberOfSources(), ndiGetIRCHKSourceXY() sensor arguments */
/*\{*/
#define  NDI_LEFT   0            /* left sensor */
#define  NDI_RIGHT  1            /* right sensor */
/*\}*/

#ifdef __cplusplus
}
#endif

#endif