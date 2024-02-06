#!/bin/sh
PACKAGE=flux-sched
USER=$(git config --get user.name)
DEBFULLNAME=$USER
EMAIL=$(git config --get user.email)
DEBEMAIL=$EMAIL

# On Debian based systems, the system Python is installed using a
# 'posix_local' scheme, which causes sysconfig to return /usr/local
# paths even if prefix=/usr. We want to ensure that Fluxion Python
# modules are installed to the same base as flux-core, so set the
# the environment variable DEB_PYTHON_INSTALL_LAYOUT=deb as described
# at https://gitlab.kitware.com/cmake/cmake/-/issues/25113.
#
# There is probably a more correct way to do this, but since these
# packages are currently just meant for testing, we do it this way
# for now:
DEB_PYTHON_INSTALL_LAYOUT=deb


SRCDIR=${1:-$(pwd)}

die() { echo "debbuild: $@" >&2; exit 1; }
log() { echo "debbuild: $@"; }

test -z "$USER" && die "User name not set in git-config"
test -z "$EMAIL" && die "User email not set in git-config"

log "Running make dist"
make dist >/dev/null || exit 1

log "Building package from latest dist tarball"
tarball=$(ls -tr *.tar.gz | tail -1)
version=$(echo $tarball | sed "s/${PACKAGE}-\(.*\)\.tar\.gz/\1/")

rm -rf debbuild
mkdir -p debbuild && cd debbuild

mv ../$tarball .

log "Unpacking $tarball"
tar xvfz $tarball >/dev/null

log "Creating debian directory and files"
cd ${PACKAGE}-${version}
cp -a ${SRCDIR}/debian . || die "failed to copy debian dir"

export DEBEMAIL DEBFULLNAME DEB_PYTHON_INSTALL_LAYOUT
log "Creating debian/changelog"
dch --create --package=$PACKAGE --newversion $version build tree release

log "Running debian-buildpackage -b"
dpkg-buildpackage -b
log "Check debbuild directory for results"
