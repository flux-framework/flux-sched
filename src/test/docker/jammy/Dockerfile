FROM fluxrm/flux-core:jammy

ARG USER=flux
ARG UID=1000
USER root

# copy scripts into image
COPY scripts/ /scripts
# Install extra buildrequires for flux-sched:
RUN apt update \
 && /scripts/install-deps.sh -y \
 && rm -rf /var/lib/apt/lists/*

# Add configured user to image with sudo access:
#
RUN /scripts/add_docker_user.sh

USER $USER
WORKDIR /home/$USER
