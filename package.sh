#!/bin/bash



VERSION=$(grep -r "VERSION" ./version.h |grep -vE "#|//" | awk -F '"' '{printf $2}')
BUILD_DIR=build
CACHE_DIR=build/cache
NAME=vpick
DIST=dist

rm -fr $BUILD_DIR
mkdir -p $CACHE_DIR
mkdir -p $CACHE_DIR/$NAME/bin
mkdir -p $CACHE_DIR/$NAME/etc/init
mkdir -p $CACHE_DIR/$NAME/meta/vpk

cp ../../../../../out/target/product/rk3588_docker/system/bin/vpick $CACHE_DIR/$NAME/bin
cp ../../../../../out/target/product/rk3588_docker/system/bin/vpkd $CACHE_DIR/$NAME/bin
cp init.vpkd.rc $CACHE_DIR/$NAME/etc/init/
cp chmod 644 $CACHE_DIR/$NAME/etc/init/init.vpkd.rc
cp -r res/vpk $CACHE_DIR/$NAME/meta

tar -zcvf $CACHE_DIR/vpick.tar.gz -C $CACHE_DIR/$NAME .
mkdir $BUILD_DIR/$NAME-$VERSION
mv $CACHE_DIR/vpick.tar.gz $BUILD_DIR/$NAME-$VERSION
cp res/install.sh $BUILD_DIR/$NAME-$VERSION
cp res/uninstall.sh $BUILD_DIR/$NAME-$VERSION
chmod 777 $BUILD_DIR/$NAME-$VERSION/install.sh
chmod 777 $BUILD_DIR/$NAME-$VERSION/uninstall.sh

tar -zcvf $BUILD_DIR/$NAME-$VERSION.tar.gz -C $BUILD_DIR $NAME-$VERSION

mkdir -p $DIST
mv $BUILD_DIR/$NAME-$VERSION.tar.gz dist

echo "************************************************************"
echo "build vpick finished!"
echo "target file: $DIST/$NAME-$VERSION.tar.gz"
echo "************************************************************"

VERSION_WIN64=$VERSION-win64
mkdir -p $BUILD_DIR/$NAME-$VERSION_WIN64/bin

cp -r res/win64/* $BUILD_DIR/$NAME-$VERSION_WIN64/
#cp res/win64/*.bat $BUILD_DIR/$NAME-$VERSION_WIN64/
cp ../../../../../out/target/product/rk3588_docker/system/bin/vpick $BUILD_DIR/$NAME-$VERSION_WIN64/bin
tar -zcvf $BUILD_DIR/$NAME-$VERSION_WIN64.tar.gz -C $BUILD_DIR $NAME-$VERSION_WIN64
mv $BUILD_DIR/$NAME-$VERSION_WIN64.tar.gz dist

echo "************************************************************"
echo "build win64 tools finished!"
echo "target file: $DIST/$NAME-$VERSION_WIN64.tar.gz"
echo "************************************************************"

