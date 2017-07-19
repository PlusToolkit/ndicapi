/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.md for details.
=========================================================Plus=header=end*/

//----------------------------------------------------------------------------
ndicapiExport bool ndiSocketOpen(const char* hostname, int port, NDISocketHandle& outSocket)
{
  return true;
}

//----------------------------------------------------------------------------
ndicapiExport void ndiSocketClose(NDISocketHandle socket)
{

}

//----------------------------------------------------------------------------
ndicapiExport bool ndiSocketFlush(NDISocketHandle socket, int flushtype)
{
  return true;
}

//----------------------------------------------------------------------------
ndicapiExport bool ndiSocketTimeout(NDISocketHandle socket, int timeoutMs)
{
  return true;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiSocketWrite(NDISocketHandle socket, const char* data, int length)
{
  return 0;
}

//----------------------------------------------------------------------------
ndicapiExport int ndiSocketRead(NDISocketHandle socket, char* reply, int numberOfBytesToRead, bool isBinary)
{
  return 1;
}

//----------------------------------------------------------------------------
ndicapiExport bool ndiSocketSleep(NDISocketHandle socket, int milliseconds)
{
  return true;
}