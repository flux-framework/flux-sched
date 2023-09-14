FROM fluxrm/flux-core:focal

ARG USER=flux
ARG UID=1000

# Install extra buildrequires for flux-sched:
RUN sudo apt-get update
RUN sudo apt-get -qq install -y --no-install-recommends \
	libboost-graph-dev \
	libboost-system-dev \
	libboost-filesystem-dev \
	libboost-regex-dev \
	python-yaml \
	libyaml-cpp-dev \
	libedit-dev \
  ninja-build \
	curl

RUN curl -s -L https://github.com/Kitware/CMake/releases/download/v3.18.5/cmake-3.18.5-linux-$(uname -m).sh > cmake.sh ;\
  sudo bash cmake.sh --prefix=/usr/local --skip-license ;\
  rm cmake.sh

# Add configured user to image with sudo access:
#
RUN \
 if test "$USER" != "flux"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo adduser $USER sudo ; \
 fi

USER $USER
WORKDIR /home/$USER
