// A simple program that connects to an NDI tracker
#include <ndicapi.h>

struct ndicapi;

int main()
{
  ndicapi* device(nullptr);
  char* name(nullptr);

  const int MAX_SERIAL_PORTS = 20;
  for (int i = 0; i < MAX_SERIAL_PORTS; ++i)
  {
    name = ndiSerialDeviceName(i);
    int result = ndiSerialProbe(name);
    if (result != NDI_OKAY)
    {
      name = nullptr;
      continue;
    }
  }

  if (name != nullptr)
  {
    device = ndiOpenSerial(name);
  }

  if (device != nullptr)
  {
    const char* reply = ndiCommand(device, "INIT");
    if (strncmp(reply, "ERROR", strlen(reply)) == 0)
    {
      return EXIT_FAILURE;
    }

    reply = ndiCommand(device, "COMM:%d%03d%d", NDI_115200, NDI_8N1, NDI_NOHANDSHAKE);

    // Add your own commands here!!!


    ndiCloseSerial(device);
  }

  return EXIT_SUCCESS;
}