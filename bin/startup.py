print("Executing startup script")
import math
import numpy as np
import scipy as sp
import os
import sys

sys.argv = ['colormotor_sandbox']

print("Trying to import cv")
import cv2


# NetworkX seems to be necessary for reload issues
print("preloading modules")
try:
    import networkx as nx
    import networkx
except:
    print('Networkx not available')

try:
    # Something really wrong going on when using anaconda, reload modules in pyrepl doesnt work properly.
    import matplotlib
except:
    print('Matplotlib not available')

try:
    #print("loading tensorflow")
    import tensorflow as tf
except:
    print('Tensorflow not available')

oldmods = set(sys.modules.keys())

def reload_modules():
    remods = []
    for mod in set(sys.modules.keys()).difference(oldmods):
        if 'matplotlib' in mod:
            continue
        if 'numpy' in mod:
            continue
        if 'scipy' in mod:
            continue
        if 'sklearn' in mod:
            continue
        remods.append(mod)
    for mod in remods:
        sys.modules.pop(mod)

def add_mod_path(modpath):
    import os, sys
    modpath = os.path.abspath(modpath)
    print('Adding ' + modpath)
        
    if not modpath in sys.path:
        sys.path.append(modpath)

def brk():
    ''' Debug utility
        Can insert this in a script to create a fake breakpoint
        all local variables will be accessible under the namespace dbg.
    '''
    import inspect
    import traceback
    global dbg
    class Dbg(object):
        def __init__(self, locs):
            for k,v in locs.items():
                self.__dict__[k] = v
                
    frame = inspect.currentframe()
    try:
        print('breaking')
        traceback.print_stack()
        print(frame.f_back.f_locals)
        dbg = Dbg(frame.f_back.f_locals)
        raise ValueError
    finally:
        del frame

global_vars = lambda: None


def params_to_globals():
    import app

    g = globals()
    for key, val in app.params.items():
        if not ' ' in key:
            g[key] = val


print("Success.")
