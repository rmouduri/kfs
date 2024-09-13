FROM debian:12-slim
# Setup of a cross-compiler for i386-elf
# https://wiki.osdev.org/GCC_Cross-Compiler

ENV PREFIX="/opt/cross"
ENV TARGET="i386-elf"
ENV PATH="$PREFIX/bin:$PATH"

# Install dependencies
RUN apt-get update && apt-get install -y \
bison           \
build-essential \
flex            \
grub2           \
libgmp3-dev     \
libmpc-dev      \
libmpfr-dev     \
make            \
nasm            \
texinfo         \
wget            \
xorriso

WORKDIR /opt

ARG BINUTILS=binutils-2.41
ARG GCC=gcc-13.2.0

# Download and extract binutils
RUN wget https://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.xz \
    && tar -xf $BINUTILS.tar.xz \
    && cd $BINUTILS \
    && mkdir build && cd build \
    && ../configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror \
    && make -j$(nproc) && make install \
    && cd ../.. && rm $BINUTILS.tar.xz

# Download and extract gcc
RUN wget https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.xz \
    && tar -xf $GCC.tar.xz \
    && cd $GCC \
    && mkdir build && cd build \
    && ../configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers \
    && make all-gcc -j$(nproc) && make all-target-libgcc -j$(nproc) && make install-gcc && make install-target-libgcc \
    && cd ../.. && rm $GCC.tar.xz

WORKDIR /workspace
