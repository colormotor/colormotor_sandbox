echo "app..."
rm app_wrap.h
rm app_wrap.cxx
rm app.py

swig -w322,362 -python -c++ -extranative  -I./ -I./../pycolormotor  app.i

echo "void initializeSwig_app() { SWIG_init(); }" >> app_wrap.cxx
echo "void initializeSwig_app();" >> app_wrap.h

echo "copying files"
cp app.py ./bin/app.py

