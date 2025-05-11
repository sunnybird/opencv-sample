CUR_DIR=$(pwd)

rm -rf build
mkdir -p build

# export CC=clang
# export CXX=clang

cmake -S ${CUR_DIR} -B ${CUR_DIR}/build -G Ninja \
-DMAKE_VERBOSE_MAKEFILE=ON \
-DCMAKE_INSTALL_PREFIX=${CUR_DIR}/build/out-br \
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
-DBUILD_SHARED_LIBS=OFF \
-DBUILD_EXAMPLES=ON \
-DBUILD_TESTS=OFF \
-DWITH_IPP=OFF \
-DWITH_TBB=OFF \
-DWITH_OPENMP=OFF \
-DWITH_PTHREADS_PF=OFF

cd build
ninja install -j16
