from distutils.core import setup, Extension
import sys


mac_indicator = 'darwin'
linux_indicator = 'linux'
platform = sys.platform
if platform.startswith(linux_indicator) or platform.startswith(mac_indicator):
    cxx_library_dir = '/usr/local/lib'
extra_link_args = []
if platform.startswith(mac_indicator):
    extra_link_args.append('-Wl')  # pass the following options to linker
    extra_link_args.append('-rpath')         # tell linker to resolve RPATH to:
    extra_link_args.append(cxx_library_dir)  # cxx_library_dir, on Mac

module1 = Extension('pyndicapi',
                    sources=[
                        'ndicapi.cxx',
                        'ndicapi_math.cxx',
                        'ndicapi_serial.cxx',
                        'ndicapi_thread.cxx',
                        'ndicapimodule.cxx',
                    ],
                    libraries=['ndicapi'],
                    extra_link_args=extra_link_args,
                    )

setup(name='pyndicapi',
      version='3.2',
      description='This package allows interfacing with NDI tracking devices',
      ext_modules=[module1]
      )
