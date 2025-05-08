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
	python3-yaml \
	llvm-dev \
	libssl-dev \
	flex \
	bison \
	cmake 

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

# Install PerfFlowAspect
RUN git clone https://github.com/flux-framework/PerfFlowAspect.git PerfFlowAspect
WORKDIR /home/$USER/PerfFlowAspect/src/c
RUN mkdir build-pfa
WORKDIR /home/$USER/PerfFlowAspect/src/c/build-pfa
RUN echo $PWD
RUN export CC="clang-15"
RUN export CXX="clang++-15"
RUN export CFLAGS="-O2 -gdwarf-4"
RUN export CXXFLAGS="-gdwarf-4"
RUN cmake -DCMAKE_CXX_COMPILER="clang++-15" -DPERFFLOWASPECT_WITH_CUDA=OFF \ 
	  -DCMAKE_INSTALL_PREFIX=/home/$USER/PerfFlowAspect/src/c/install-pfa ../
RUN make 
RUN make install

WORKDIR /home/$USER


