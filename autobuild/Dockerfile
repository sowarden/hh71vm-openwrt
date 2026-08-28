FROM ubuntu:22.04@sha256:2edbbc5dc405e9612ba3584ce95480277e3eb374407b5505fe26f17df77c7dbc
ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8 LC_ALL=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential clang flex bison gawk gettext git libncurses-dev libssl-dev \
    libelf-dev python2 python3 rsync unzip zip zlib1g-dev file wget curl time \
    subversion swig gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi lua5.1 \
    ca-certificates ccache perl cmake pkg-config squashfs-tools xz-utils bzip2 \
    patch diffutils libtool autoconf automake && rm -rf /var/lib/apt/lists/*
RUN mkdir /build /output /cache && chmod 1777 /build /output /cache
ARG BUILD_UID=1000
ARG BUILD_GID=1000
RUN groupadd --gid "$BUILD_GID" build && \
    useradd --uid "$BUILD_UID" --gid "$BUILD_GID" --create-home --home-dir /home/build build
USER build
ENV HOME=/home/build PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    PYTHONDONTWRITEBYTECODE=1 CCACHE_DIR=/cache/ccache HH71VM_CCACHE_DIR=/cache/ccache \
    CCACHE_BASEDIR=/build CCACHE_MAXSIZE=1G CCACHE_COMPILERCHECK=content
WORKDIR /build
ENTRYPOINT ["python3", "/src/autobuild/build.py"]
