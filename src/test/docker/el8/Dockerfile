FROM fluxrm/flux-core:el8

ARG USER=flux
ARG UID=1000

# Install extra buildrequires for flux-sched:
RUN sudo yum -y install \
	boost-devel \
	boost-graph \
	boost-system \
	boost-filesystem \
	boost-regex \
	libxml2-devel \
	readline-devel \
	python3-pyyaml \
	yaml-cpp-devel \
	libedit-devel

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
