FROM fluxrm/flux-core:el9

ARG USER=flux
ARG UID=1000
USER root

# copy scripts into image
COPY scripts/ /scripts
# Install extra buildrequires for flux-sched:
RUN sudo /scripts/install-deps.sh -y --allowerasing gcc-toolset-13-{gcc,gcc-c++,gdb} \
 && sudo dnf clean all

# Add configured user to image with sudo access:
#
RUN /scripts/add_docker_user.sh

USER $USER
WORKDIR /home/$USER
ENV CHECK_RUN_SOURCE_ENV=/opt/rh/gcc-toolset-13/enable
