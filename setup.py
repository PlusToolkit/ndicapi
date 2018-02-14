from distutils.core import setup, Extension


module1 = Extension('pyndicapi',
                    sources = [
                        'ndicapi.cxx',
                        'ndicapi_math.cxx',
                        'ndicapi_serial.cxx',
                        'ndicapi_thread.cxx',
                        'ndicapimodule.cxx',
                    ],
                    libraries = ['ndicapi'],
                   )
setup (name = 'pyndicapi',
       version = '3.2',
       description = 'This package allows interfacing with NDI tracking devices',
       ext_modules = [module1])
