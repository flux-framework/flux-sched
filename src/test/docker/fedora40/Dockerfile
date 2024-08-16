FROM fluxrm/flux-core:fedora40

LABEL maintainer="Tom Scogland <scogland1@llnl.gov>"

ARG USER=flux
ARG UID=1000

#  Install flux-sched dependencies
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
	cmake \
	clang \
	lcov \
 && sudo dnf clean all

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
