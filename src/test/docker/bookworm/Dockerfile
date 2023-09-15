FROM fluxrm/flux-core:bookworm

ARG USER=flux
ARG UID=1000
USER root

# Install extra buildrequires for flux-sched:
RUN apt-get update
RUN apt-get -qq install -y --no-install-recommends \
	libboost-graph-dev \
	libboost-system-dev \
	libboost-filesystem-dev \
	libboost-regex-dev \
	libyaml-cpp-dev \
	libedit-dev \
	ninja-build \
	python3-yaml

# Add configured user to image with sudo access:
#
RUN \
 if test "$USER" != "flux"; then  \
      groupadd -g $UID $USER \
   && useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && adduser $USER sudo ; \
 fi

USER $USER
WORKDIR /home/$USER
