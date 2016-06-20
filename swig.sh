echo "app..."
rm app_wrap.h
rm app_wrap.cxx
rm app.py
rm ui_wrap.h
rm ui_wrap.cxx
rm ui.py

echo "App bindings"
swig -w322,362 -python -c++ -extranative  -I./ -I./../colormotor/addons/pycolormotor  app.i 

echo "void initializeSwig_app() { SWIG_init(); }" >> app_wrap.cxx
echo "void initializeSwig_app();" >> app_wrap.h

echo "copying files"
cp app.py ./bin/app.py
rm -f ./bin/libpycm.dylib
cp ../colormotor/addons/pycolormotor/modules/libpycm.dylib