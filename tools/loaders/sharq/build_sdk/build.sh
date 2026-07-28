prefix=$1

if [ -z "$prefix" ]; then
    echo "Error: argument is required. Indicate build output path."
    exit 1
fi


mkdir -p cloud-sdks-build
cd cloud-sdks-build
cmake -DCMAKE_INSTALL_PREFIX=$prefix .. && make 