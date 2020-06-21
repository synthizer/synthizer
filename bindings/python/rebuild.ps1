rd -recurse build
rd *.pyd
python setup.py build_ext
python setup.py develop
