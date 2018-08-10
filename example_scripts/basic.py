import math
from cm import *
import app

class App:
    def __init__(self):
        app.addFloat('foo',0.0,0.0,1.0)
        self.t = 0.0

    def open(self):
        p = app.openFileDialog('txt')
        if p:
            print p
            
    def init(self):
        print("Init")

    def exit(self):
        print("Exit")

    def frame(self):
        f = app.getFloat('foo')
        clear(f, f, f, 1.)
        