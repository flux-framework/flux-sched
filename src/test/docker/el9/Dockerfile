FROM fluxrm/flux-core:el9

ARG USER=flux
ARG UID=1000

# Install extra buildrequires for flux-sched:
RUN sudo dnf -y install \
	boost-devel \
	boost-graph \
	boost-system \
	boost-filesystem \
	boost-regex \
	readline-devel \
	python3-pyyaml \
	yaml-cpp-devel \
	libedit-devel \
	ninja-build \
	cmake \
	curl

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
