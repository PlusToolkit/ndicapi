from distutils.core import setup, Extension


module1 = Extension('ndicapi',
                    sources = [
'ndicapi.cxx',
'ndicapi_math.cxx',
'ndicapi_serial.cxx',
'ndicapi_thread.cxx',
'ndicapimodule.cxx',
])

setup (name = 'ndicapi',
       version = '3.2',
       description = 'This package allows interfacing with NDI tracking devices',
       ext_modules = [module1])
