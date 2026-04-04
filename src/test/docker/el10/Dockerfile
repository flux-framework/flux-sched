FROM fluxrm/flux-core:el10

ARG USER=flux
ARG UID=1000
USER root

# copy scripts into image
COPY scripts/ /scripts
# Install extra buildrequires for flux-sched:
RUN /scripts/install-deps.sh -y --allowerasing \
 && dnf clean all

# Add configured user to image with sudo access:
#
RUN /scripts/add_docker_user.sh

USER $USER
WORKDIR /home/$USER
