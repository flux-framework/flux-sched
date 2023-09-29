FROM fluxrm/flux-core:bookworm

LABEL maintainer="Vanessasaurus <@vsoch>"

# Match the default user id for a single system so we aren't root
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
ENV USERNAME=${USERNAME}
ENV USER_UID=${USER_UID}
ENV USER_GID=${USER_GID}
USER root

# Install extra buildrequires for flux-sched:
RUN sudo apt-get update
RUN sudo apt-get -qq install -y --no-install-recommends \
        libboost-graph-dev \
        libboost-system-dev \
        libboost-filesystem-dev \
        libboost-regex-dev \
        python3-yaml \
        libyaml-cpp-dev \
        libedit-dev \
        ninja-build \
        curl

# Assuming installing to /usr/local
ENV LD_LIBRARY_PATH=/usr/lib:/usr/local/lib

RUN curl -s -L https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4-linux-$(uname -m).sh > cmake.sh ;\
    sudo bash cmake.sh --prefix=/usr/local --skip-license ;\
    rm cmake.sh

# Install Go 19 for TBA bindings (if Go bindings desired)
RUN wget https://go.dev/dl/go1.19.10.linux-amd64.tar.gz  && tar -xvf go1.19.10.linux-amd64.tar.gz && \
         mv go /usr/local && rm go1.19.10.linux-amd64.tar.gz
      
ENV PATH=$PATH:/usr/local/go/bin:/home/vscode/go/bin

# extra interactive utilities
RUN apt-get update \
 && apt-get -qq install -y --no-install-recommends \
    fd-find \
    less \
    bats \
    ripgrep

# Add the group and user that match our ids
RUN groupadd -g ${USER_GID} ${USERNAME} && \
    adduser --disabled-password --uid ${USER_UID} --gid ${USER_GID} --gecos "" ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers
USER $USERNAME
