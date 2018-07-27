// A simple program that connects to an NDI tracker
#include <ndicapi.h>
#include <cstring>

#include <iostream>
#if _MSC_VER >= 1700
  #include <vector>
  #include <algorithm>
  #include <future>
#endif

#if _MSC_VER >= 1700
//----------------------------------------------------------------------------
bool ParallelProbe(ndicapi*& outDevice, bool checkDSR)
{
  const int MAX_SERIAL_PORT_NUMBER = 20; // the serial port is almost surely less than this number
  std::vector<bool> deviceExists(MAX_SERIAL_PORT_NUMBER);
  std::fill(begin(deviceExists), end(deviceExists), false);
  std::vector<std::future<void>> tasks;
  std::mutex deviceNameMutex;
  for (int i = 0; i < MAX_SERIAL_PORT_NUMBER; i++)
  {
    std::future<void> result = std::async([i, &deviceNameMutex, &deviceExists]()
    {
      std::string devName;
      {
        std::lock_guard<std::mutex> guard(deviceNameMutex);
        devName = std::string(ndiSerialDeviceName(i));
      }
      int errnum = ndiSerialProbe(devName.c_str(),checkDSR);
      if (errnum == NDI_OKAY)
      {
        deviceExists[i] = true;
      }
    });
    tasks.push_back(std::move(result));
  }
  for (int i = 0; i < MAX_SERIAL_PORT_NUMBER; i++)
  {
    tasks[i].wait();
  }
  for (int i = 0; i < MAX_SERIAL_PORT_NUMBER; i++)
  {
    // use first device found
    if (deviceExists[i] == true)
    {
      char* devicename = ndiSerialDeviceName(i);
      outDevice = ndiOpenSerial(devicename);
      return true;
    }
  }

  return false;
}
#endif

struct ndicapi;

int main(int argc, char * argv[])
{
  bool checkDSR = false;
  ndicapi* device(nullptr);
  const char* name(nullptr);

  if(argc > 1)
    name = argv[1];
  else
  {
#if _MSC_VER >= 1700
    ParallelProbe(device,argc > 1 ? argv[1]: 0, checkDSR);
#else
    {
      const int MAX_SERIAL_PORTS = 20;
      for (int i = 0; i < MAX_SERIAL_PORTS; ++i)
      {
        name = ndiSerialDeviceName(i);
        int result = ndiSerialProbe(name,checkDSR);
        if (result == NDI_OKAY)
        {
          break;
        }
      }
    }
#endif
  }

  if (name != nullptr)
  {
    device = ndiOpenSerial(name);
  }

  if (device != nullptr)
  {
    const char* reply = ndiCommand(device, "INIT:");
    if (strncmp(reply, "ERROR", strlen(reply)) == 0 || ndiGetError(device) != NDI_OKAY)
    {
      std::cerr << "Error when sending command: " << ndiErrorString(ndiGetError(device)) << std::endl;
      return EXIT_FAILURE;
    }

    reply = ndiCommand(device, "COMM:%d%03d%d", NDI_115200, NDI_8N1, NDI_NOHANDSHAKE);

    // Add your own commands here!!!


    ndiCloseSerial(device);
  }

  return EXIT_SUCCESS;
}