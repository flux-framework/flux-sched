FROM fluxrm/flux-core:fedora40

LABEL maintainer="Tom Scogland <scogland1@llnl.gov>"

ARG USER=flux
ARG UID=1000
USER root

# copy scripts into image
COPY scripts/ /scripts
#  Install flux-sched dependencies
RUN /scripts/install-deps.sh -y \
 && dnf clean all

# Add configured user to image with sudo access:
#
RUN /scripts/add_docker_user.sh

USER $USER
WORKDIR /home/$USER
