import logging
import time

import click

from ndicapy import *


class NDIPolarisTracker:
    def __init__(self):
        self.device = None

    def connect(self, rom_file):
        result = ''
        for port in range(20):
            devicename = ndiDeviceName(port)
            if not devicename:
                continue
            result = ndiProbe(devicename)
            if result == NDI_OKAY:
                break
        if result != NDI_OKAY:
            logging.error('Failed to probe NDI serial devices')
            return False
        self.device = ndiOpen(devicename)
        if not self.device:
            logging.error('Failed to open NDI device')
            return False
        reply = ndiINIT(self.device)
        error = ndiGetError(self.device)
        if reply.startswith('ERROR') or error != NDI_OKAY:
            logging.error('Failed to initialize NDI tracker')
            return False
        reply = ndiCOMM(self.device, NDI_115200, NDI_8N1, NDI_NOHANDSHAKE)

        ndiCommand(self.device, 'PHRQ:*********1****')
        port_handle = ndiGetPHRQHandle(self.device)

        reply = ndiPVWRFromFile(self.device, port_handle, rom_file)
        error = ndiGetError(self.device)

        reply = ndiCommand(self.device, f'PINIT:{port_handle:02X}')
        error = ndiGetError(self.device)

        reply = ndiCommand(self.device, f'PENA:{port_handle:02X}D')
        error = ndiGetError(self.device)

        reply = ndiTSTART(self.device)
        error = ndiGetError(self.device)
        if reply.startswith('ERROR') or error != NDI_OKAY:
            logging.error('NDI: Failed to start tracking')
        return True

    def disconnect(self):
        if self.device:
            ndiTSTOP(self.device)
            ndiClose(self.device)
        return True

    def measure(self):
        if not self.device:
            logging.error('Attempted to measure although NDI tracker not available.')
            return
        # get tool transform
        reply = ndiCommand(self.device, 'BX:0801')
        transform = ndiGetBXTransform(self.device, port)
        if transform in ('MISSING', 'DISABLED'):
            return
        ret = ndiGetBXPortStatus(self.device, port)
        frameindex = ndiGetBXFrame(self.device, port)
        return transform


@click.command()
@click.argument('romfile')
def main(romfile):
    tracker = NDIPolarisTracker()
    tracker.connect(romfile)
    while True:
        m = tracker.measure()
        if m is not None:
            print(m)
    tracker.disconnect()


if __name__ == '__main__':
    main()
