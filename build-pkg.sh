#!/bin/sh
# build-pkg.sh - Build PizzaFool SVR4 package on Solaris
# Run this on the SPARCstation 4 (or any Solaris 7 SPARC system)
#
# Usage: sh build-pkg.sh
#
# Produces: pizzafool-1.0-sparc.pkg

PKGNAME="JWpzfool"
VERSION="1.0"
BASEDIR="/opt/pizzafool"
SRCDIR=`pwd`
BUILDDIR="/tmp/pizzafool-build"
STAGEDIR="/tmp/pizzafool-stage"
PKGDIR="/tmp/pizzafool-pkg"

echo "=== PizzaFool SVR4 Package Builder ==="
echo "Source: ${SRCDIR}"
echo ""

# Clean previous builds
rm -rf ${BUILDDIR} ${STAGEDIR} ${PKGDIR} /tmp/pizzafool-spool
mkdir -p ${BUILDDIR}
mkdir -p ${STAGEDIR}${BASEDIR}/bin
mkdir -p ${STAGEDIR}${BASEDIR}/share/images
mkdir -p ${STAGEDIR}${BASEDIR}/man/man6
mkdir -p ${PKGDIR}
mkdir -p /tmp/pizzafool-spool

# Copy source and build
echo "--- Compiling ---"
cp ${SRCDIR}/pizzafool.c ${SRCDIR}/Makefile ${BUILDDIR}/
cd ${BUILDDIR}
make clean 2>/dev/null
make
if [ $? -ne 0 ]; then
    echo "BUILD FAILED"
    exit 1
fi
echo "Build OK"

# Stage files
echo "--- Staging ---"
cp ${BUILDDIR}/pizzafool ${STAGEDIR}${BASEDIR}/bin/
cp ${SRCDIR}/images/*.xpm ${STAGEDIR}${BASEDIR}/share/images/
cp ${SRCDIR}/pizzafool.6 ${STAGEDIR}${BASEDIR}/man/man6/
cp ${SRCDIR}/README ${STAGEDIR}${BASEDIR}/
cp ${SRCDIR}/README.md ${STAGEDIR}${BASEDIR}/
echo "Staged to ${STAGEDIR}"

# Generate prototype file
echo "--- Generating prototype ---"
PSTAMP=`date '+%Y%m%d%H%M%S'`
cat > ${PKGDIR}/pkginfo <<EOF
PKG=${PKGNAME}
NAME=PizzaFool - Mr. T's Pizza Ordering System
ARCH=sparc
VERSION=${VERSION}
CATEGORY=application
VENDOR=Julian Wolfe
EMAIL=
PSTAMP=${PSTAMP}
BASEDIR=${BASEDIR}
CLASSES=none
EOF

cp ${SRCDIR}/pkg/depend ${PKGDIR}/
cp ${SRCDIR}/pkg/postinstall ${PKGDIR}/
cp ${SRCDIR}/pkg/preremove ${PKGDIR}/ 2>/dev/null

# Build prototype from staged directory
cat > ${PKGDIR}/prototype <<PROTO
i pkginfo
i depend
i postinstall
PROTO

# Add preremove if it exists
if [ -f ${PKGDIR}/preremove ]; then
    echo "i preremove" >> ${PKGDIR}/prototype
fi

# Add directories and files
echo "d none bin 0755 root bin" >> ${PKGDIR}/prototype
echo "f none bin/pizzafool 0755 root bin" >> ${PKGDIR}/prototype
echo "d none share 0755 root bin" >> ${PKGDIR}/prototype
echo "d none share/images 0755 root bin" >> ${PKGDIR}/prototype
for img in ${STAGEDIR}${BASEDIR}/share/images/*.xpm; do
    bname=`basename $img`
    echo "f none share/images/${bname} 0644 root bin" >> ${PKGDIR}/prototype
done
echo "d none man 0755 root bin" >> ${PKGDIR}/prototype
echo "d none man/man6 0755 root bin" >> ${PKGDIR}/prototype
echo "f none man/man6/pizzafool.6 0644 root bin" >> ${PKGDIR}/prototype
echo "f none README 0644 root bin" >> ${PKGDIR}/prototype
echo "f none README.md 0644 root bin" >> ${PKGDIR}/prototype

echo "Prototype:"
cat ${PKGDIR}/prototype
echo ""

# Build package
echo "--- Building package ---"
pkgmk -o -r ${STAGEDIR}${BASEDIR} -d /tmp/pizzafool-spool -f ${PKGDIR}/prototype

# Convert to datastream
echo "--- Creating datastream package ---"
OUTPKG="${SRCDIR}/pizzafool-${VERSION}-sparc.pkg"
pkgtrans -s /tmp/pizzafool-spool ${OUTPKG} ${PKGNAME}

echo ""
echo "=== Package built successfully ==="
echo "Output: ${OUTPKG}"
ls -la ${OUTPKG}
echo ""
echo "Install with: pkgadd -d ${OUTPKG}"

# Cleanup
rm -rf "${BUILDDIR}" "${STAGEDIR}" "${PKGDIR}" /tmp/pizzafool-spool
