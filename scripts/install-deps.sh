#!/bin/sh

if [ -z "$ID" ] ; then
	while IFS== read -r key value; do
		value="${value%\"}"
		value="${value#\"}"
		export "$key"="$value"
	done </etc/os-release
fi

PM=""
case "$ID" in
	ubuntu|debian)
		PM=apt
		DEV_SUFFIX="-dev"
		DEV_SUFFIX_APT="-dev"
		PY3_PREFIX="python3-"
		LIB_PREFIX="lib"
		;;
	fedora|rocky|alma|rhel|centos)
		PM=dnf
		DEV_SUFFIX="-devel"
		PY3_PREFIX="python3-"
		LIB_PREFIX=""
		;;
	alpine)
		PM=apk
		DEV_SUFFIX="-dev"
		PY3_PREFIX="py3-"
		LIB_PREFIX=""
		;;
esac

[ -z "$PM" ] && echo Package manager for $ID is unknown && exit 1

pkglist="
${LIB_PREFIX}boost${DEV_SUFFIX}
${LIB_PREFIX}boost-graph${DEV_SUFFIX_APT}
libedit${DEV_SUFFIX}
${LIB_PREFIX}hwloc${DEV_SUFFIX}
${PY3_PREFIX}yaml
${LIB_PREFIX}yaml-cpp${DEV_SUFFIX}
ninja-build
cmake
curl
bash
jq
valgrind
"

case $PM in
	apt)
		CMD="apt install ${@} ${pkglist}"
		;;
	apk)
		CMD="apk add ${@} ${pkglist}"
		;;
	dnf)
		CMD="dnf install ${@} ${pkglist}"
		;;
esac
echo will run "'$CMD'"
if [ $(id -u) -ne 0 ]
  then echo command will not work without root, re-run as root
  exit 1
fi
$CMD
