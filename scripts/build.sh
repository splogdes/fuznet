BUILD_DIR=${BUILD_DIR:-build}

mkdir -p $BUILD_DIR

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cd $BUILD_DIR
    cmake ..
    cd -
else
    echo "CMake has already been run."
fi

cd $BUILD_DIR
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

exit 0