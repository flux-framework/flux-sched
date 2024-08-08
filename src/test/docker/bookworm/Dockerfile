FROM fluxrm/flux-core:bookworm

ARG USER=flux
ARG UID=1000
USER root

# Install extra buildrequires for flux-sched:
RUN apt-get update
RUN apt-get -qq install -y --no-install-recommends \
	libboost-graph1.81-dev \
	libboost-json1.81-dev \
	libboost-system1.81-dev \
	libboost-filesystem1.81-dev \
	libboost-regex1.81-dev \
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
