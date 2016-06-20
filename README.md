# Colormotor Python Sandbox
Live reloading sandbox for PyColormotor. 

![alt tag](https://raw.githubusercontent.com/colormotor/pycm_sandbox_gl/master/shot.jpg)

### Prerequisites
First generate the Colormotor Python bindings by following the instructions here:
[https://github.com/colormotor/colormotor/tree/master/addons/pycolormotor](https://github.com/colormotor/colormotor/tree/master/addons/pycolormotor)

To simplify the installation, place the repository directory at the same level as *colomotor*. Then (from the *pycm_sandbox_gl* directory) generate the sandbox bindings with:

```
sh swig.sh
```

### Compiling
#### OSX
Open the XCode project and compile. If the code has been installed at the same directory level as *colormotor*, no modifications should be necessary.

