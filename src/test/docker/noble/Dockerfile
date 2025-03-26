FROM ubuntu:noble

LABEL maintainer="James Corbett <corbett8@llnl.gov>"

ARG USER=flux
ARG UID=1000
ARG GID=1000

# Utilities
RUN apt-get update \
# install latest pkg utils:
 && apt-get -qq install -y --no-install-recommends \
        apt-utils \
 && apt-get -qq install -y --no-install-recommends \
        locales \
        ca-certificates \
        wget \
        man \
        git \
        flex \
        ssh \
        sudo \
        vim \
        lcov \
        ccache \
        lua5.2 \
        mpich \
        libmpich-dev \
        valgrind \
        jq \
# Compilers, autotools
        build-essential \
        pkg-config \
        libtool \
        autoconf \
        automake \
        make \
        cmake \
        ninja-build \
        clang-18 \
        clang-tools-18 \
        zlib1g \
        zlib1g-dev \
        mpi \
        pdsh \
# default version
        gcc-13 \
        g++-13 \
        gfortran \
# newest
        gcc-14 \
        g++-14 \
## python 3.12
        python3-dev \
 && apt-get -qq purge -y python3-pip \
 && apt-get -qq autoremove -y \
 && rm -rf /var/lib/apt/lists/*

# Ubuntu containers now "minimize" themselves, so manpages aren't installed.
# To warn people about this, /usr/bin/man is a shell script that doesn't bother
# to look for the man page, but just prints a warning.  Link /usr/bin/man.REAL
# to /usr/bin/man to make this behave
RUN ln -sf /usr/bin/man.REAL /usr/bin/man

ENV LANG=C.UTF-8

# the image has a UID 1000 named ubuntu now for some reason, fix it
RUN userdel ubuntu
# noble defaults to an invalid TZ /UTC
ENV TZ="America/Los_Angeles"

# copy scripts into image
COPY src/test/docker/add_docker_user.sh /etc/add_docker_user.sh
# Add configured user to image with sudo access:
RUN /etc/add_docker_user.sh

USER $USER
WORKDIR /home/$USER
