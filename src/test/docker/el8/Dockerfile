FROM fluxrm/flux-core:el8

ARG USER=flux
ARG UID=1000

# Install extra buildrequires for flux-sched:
RUN sudo dnf -y install \
	boost-devel \
	boost-graph \
	boost-system \
	boost-filesystem \
	boost-regex \
	python3-pyyaml \
	yaml-cpp-devel \
	libedit-devel \
	ninja-build \
	gcc-toolset-12-{gcc,gcc-c++,gdb} \
	curl

RUN curl -s -L https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4-linux-$(uname -m).sh > cmake.sh ;\
  sudo bash cmake.sh --prefix=/usr/local --skip-license ;\
  rm cmake.sh

# Add configured user to image with sudo access:
#
RUN \
 if test "$USER" != "flux"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo usermod -G wheel $USER; \
 fi

USER $USER
WORKDIR /home/$USER
ENV CHECK_RUN_SOURCE_ENV=/opt/rh/gcc-toolset-12/enable
