FROM fluxrm/flux-core:alpine

ARG USER=flux
ARG UID=1000
USER root

# Install extra buildrequires for flux-sched:
COPY scripts/ /scripts
RUN sudo /scripts/install-deps.sh

# Add configured user to image with sudo access:
#
RUN /scripts/add_docker_user.sh

USER $USER
WORKDIR /home/$USER

