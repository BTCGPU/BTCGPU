#!/usr/bin/env bash

set -ex


locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8

export LANG='en_US.UTF-8'
export LC_ALL='en_US.UTF-8'
umask 002
export OUTDIR=/bitcoin/out
rm -rf $OUTDIR
mkdir $OUTDIR
chmod a+rwx $OUTDIR

MAKEOPTS=(-j$1)


WRAP_DIR=$HOME/wrapped
HOSTS="x86_64-linux-gnu"
CONFIGFLAGS="--enable-glibc-back-compat --enable-reduce-exports --disable-bench --disable-gui-tests --disable-wallet --with-gui=no --disable-tests --disable-bench"
HOST_CFLAGS="-O2 -g"
HOST_CXXFLAGS="-O2 -g"
HOST_LDFLAGS=-static-libstdc++

export QT_RCC_TEST=1
export GZIP="-9n"
export TZ="UTC"
export BUILD_DIR=`pwd`
mkdir -p ${WRAP_DIR}
if test -n "$GBUILD_CACHE_ENABLED"; then
	export SOURCES_PATH=${GBUILD_COMMON_CACHE}
	export BASE_CACHE=${GBUILD_PACKAGE_CACHE}
	mkdir -p ${BASE_CACHE} ${SOURCES_PATH}
fi

export PATH_orig=${PATH}
export PATH=${WRAP_DIR}:${PATH}

EXTRA_INCLUDES_BASE=$WRAP_DIR/extra_includes
mkdir -p $EXTRA_INCLUDES_BASE

# x86 needs /usr/include/i386-linux-gnu/asm pointed to /usr/include/x86_64-linux-gnu/asm,
# but we can't write there. Instead, create a link here and force it to be included in the
# search paths by wrapping gcc/g++.

mkdir -p $EXTRA_INCLUDES_BASE/i686-pc-linux-gnu
rm -f $WRAP_DIR/extra_includes/i686-pc-linux-gnu/asm
ln -s /usr/include/x86_64-linux-gnu/asm $EXTRA_INCLUDES_BASE/i686-pc-linux-gnu/asm

for prog in gcc g++; do
rm -f ${WRAP_DIR}/${prog}
cat << EOF > ${WRAP_DIR}/${prog}
#!/bin/bash
REAL="`which -a ${prog} | grep -v ${WRAP_DIR}/${prog} | head -1`"
for var in "\$@"
do
	if [ "\$var" = "-m32" ]; then
		export C_INCLUDE_PATH="$EXTRA_INCLUDES_BASE/i686-pc-linux-gnu"
		export CPLUS_INCLUDE_PATH="$EXTRA_INCLUDES_BASE/i686-pc-linux-gnu"
		break
	fi
done
\$REAL \$@
EOF
chmod +x ${WRAP_DIR}/${prog}
done

cd bitcoin
BASEPREFIX=`pwd`/depends
# Build dependencies for each host
for i in $HOSTS; do
	EXTRA_INCLUDES="$EXTRA_INCLUDES_BASE/$i"
	if [ -d "$EXTRA_INCLUDES" ]; then
		export HOST_ID_SALT="$EXTRA_INCLUDES"
	fi
	make ${MAKEOPTS} -C ${BASEPREFIX} NO_QT=1 NO_WALLET=1 HOST="${i}" boost_download_path='https://www.dropbox.com/s/vgn8gn04xyja128/boost_1_63_0.tar.bz2?dl=1'
	unset HOST_ID_SALT
done

export PATH=${PATH_orig}
export PATH=${WRAP_DIR}:${PATH}

# Create the release tarball using (arbitrarily) the first host
./autogen.sh
CONFIG_SITE=${BASEPREFIX}/`echo "${HOSTS}" | awk '{print $1;}'`/share/config.site ./configure --prefix=/
make dist
SOURCEDIST=`echo bitcoin-*.tar.gz`
DISTNAME=`echo ${SOURCEDIST} | sed 's/.tar.*//'`
# Correct tar file order
mkdir -p temp
pushd temp
tar xf ../$SOURCEDIST
find bitcoin-* | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ../$SOURCEDIST
popd

ORIGPATH="$PATH"
# Extract the release tarball into a dir for each host and build
for i in ${HOSTS}; do
	export PATH=${BASEPREFIX}/${i}/native/bin:${ORIGPATH}
	mkdir -p distsrc-${i}
	cd distsrc-${i}
	INSTALLPATH=`pwd`/installed/${DISTNAME}
	mkdir -p ${INSTALLPATH}
	tar --strip-components=1 -xf ../$SOURCEDIST

	CONFIG_SITE=${BASEPREFIX}/${i}/share/config.site ./configure --prefix=/ --disable-ccache  --disable-maintainer-mode --disable-dependency-tracking ${CONFIGFLAGS} CFLAGS="${HOST_CFLAGS}" CXXFLAGS="${HOST_CXXFLAGS}" LDFLAGS="${HOST_LDFLAGS}"
	make ${MAKEOPTS}
	make ${MAKEOPTS} -C src check-security

	make install DESTDIR=${INSTALLPATH}
	cd installed
	find . -name "lib*.la" -delete
	find . -name "lib*.a" -delete
	rm -rf ${DISTNAME}/lib/pkgconfig
	find ${DISTNAME}/bin -type f -executable -exec ../contrib/devtools/split-debug.sh {} {} {}.dbg \;
	find ${DISTNAME}/lib -type f -exec ../contrib/devtools/split-debug.sh {} {} {}.dbg \;
	find ${DISTNAME} -not -name "*.dbg" | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ${OUTDIR}/${DISTNAME}-${i}.tar.gz
	find ${DISTNAME} -name "*.dbg" | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ${OUTDIR}/${DISTNAME}-${i}-debug.tar.gz
	cd ../../
	rm -rf distsrc-${i}
done
mkdir -p $OUTDIR/src
mv $SOURCEDIST $OUTDIR/src


