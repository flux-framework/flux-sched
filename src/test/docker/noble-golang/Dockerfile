FROM fluxrm/flux-core:noble

ARG USER=flux
ARG UID=1000

# Install extra buildrequires for flux-sched:
RUN sudo apt-get update
RUN sudo apt-get -qq install -y --no-install-recommends \
    libboost-graph-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-regex-dev \
    python3-yaml \
    libyaml-cpp-dev \
    ninja-build \
    curl \
    libedit-dev

# Install cmake for new build system
RUN curl -s -L https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4-linux-$(uname -m).sh > cmake.sh ;\
  sudo bash cmake.sh --prefix=/usr/local --skip-license ;\
  rm cmake.sh

# Install Golang 1.19.10 for binding tests
RUN wget https://go.dev/dl/go1.19.10.linux-amd64.tar.gz
RUN sudo rm -rf /usr/local/go \
    && sudo tar -C /usr/local -xzf go1.19.10.linux-amd64.tar.gz \
    && rm -f go1.19.10.linux-amd64.tar.gz

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
ENV PATH=$PATH:/usr/local/go/bin
