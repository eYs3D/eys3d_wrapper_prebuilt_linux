export EYS3D_HOME=./eYs3D/
echo $EYS3D_HOME

rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make install

