set -e
sudo apt install cmake curl libssl-dev -y

VERS="3.21.3"
SRC="https://github.com/Kitware/CMake/releases/download/v$VERS"
HASH="cmake-$VERS-SHA-256.txt"
DIR="cmake-$VERS"
ARCHIVE="$DIR.tar.gz"

curl -OL "$SRC/$HASH"
curl -OL "$SRC/$ARCHIVE"

sha256sum -c --ignore-missing $HASH

curl -OL "$SRC/$HASH.asc"
#gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys C6C265324BBEBDC350B513D02D2CEF1034921684
#gpg --verify "$HASH.asc" "$HASH"

rm "$HASH" "$HASH.asc"

tar -xvzf "$ARCHIVE"
rm "$ARCHIVE"

cd "$DIR"
#cmake .
#make

#sudo make install
cd ..
sudo rm -rf "$DIR"
