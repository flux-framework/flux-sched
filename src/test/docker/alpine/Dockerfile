FROM fluxrm/flux-core:alpine

ARG USER=flux
ARG UID=1000
USER root

# Install extra buildrequires for flux-sched:
RUN sudo apk add \
	boost-dev \
	py3-yaml \
	yaml-cpp-dev \
	libedit-dev \
	ninja-build \
	cmake \
	bash \
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

