FROM fluxrm/flux-core:noble

ARG USER=flux
ARG UID=1000
USER root

# copy scripts into image
COPY scripts/ /scripts
# Install extra buildrequires for flux-sched:
RUN sudo apt update \
 && sudo /scripts/install-deps.sh -y \
 && sudo rm -rf /var/lib/apt/lists/*

# Add configured user to image with sudo access:
#
RUN /scripts/add_docker_user.sh

USER $USER
WORKDIR /home/$USER
