
@echo on

mkdir project
cd project
"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" ..
cd ..

mkdir bin
cd bin
echo "Do not delete me">do_not_delete.txt
cd ..

mkdir data
cd data
echo "Do not delete me">do_not_delete.txt
cd ..