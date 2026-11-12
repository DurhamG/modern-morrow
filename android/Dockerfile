# Builds OpenMW's dependencies for Android arm64 into /root/prefix.
#
# Based on the Dockerfile of openmw-android-docker by cavebros and Duron27
# (https://gitlab.com/cavebros/openmw-android-docker), licensed under the GNU GPL version 3. This copy keeps only what
# OpenMW needs, pins every source, and builds the OpenSceneGraph commit that extern/CMakeLists.txt pins.
#
# The NDK ships Linux tools only for x86-64, so the image is linux/amd64. On Apple Silicon, Docker Desktop runs it
# under Rosetta. From the repository root:
#
#   docker build --platform linux/amd64 -t modernmorrow-android-deps android
FROM fedora:43

ENV BUILD_TYPE=Release
ENV API=26
ENV ABI=arm64-v8a
ENV ARCH=aarch64

ARG NDK_VERSION=r29
ARG LIBICU_VERSION=70-1
ARG BZIP2_VERSION=1ea1ac188ad4b9cb662e3f8314673c63df95a589
ARG ZLIB_VERSION=1.3.1
ARG LIBJPEG_TURBO_VERSION=3.1.0
ARG LIBPNG_VERSION=1.6.48
ARG FREETYPE2_VERSION=2.13.3
ARG LIBXML2_VERSION=2.14.3
ARG OBOE_VERSION=1.9.3
ARG OPENAL_VERSION=1.24.3
ARG BOOST_VERSION=1.88.0
ARG FFMPEG_VERSION=7.1.1
ARG SDL2_VERSION=2.32.4
ARG BULLET_VERSION=3.25
ARG GL4ES_VERSION=a26bd21c29126039c08f4bae60f47a6bf79ed786
ARG MYGUI_VERSION=3.4.3
ARG LZ4_VERSION=1.10.0
ARG LUAJIT_VERSION=63c8f1a9b479ba5518cf6a170ff7729178bc3cfd
ARG COLLADA_DOM_VERSION=2.5.0
ARG OSG_VERSION=9b384d5439977784e5cd250ff8ff7966928ddd27

# Where gl4es reads its config file and writes its log on the device.
ARG GL4ES_DIRECTORY=/sdcard/Android/data/io.github.durhamg.modernmorrow/files/config

ENV NDK_TRIPLET=${ARCH}-linux-android
ENV ANDROID_NDK=/root/Android/android-ndk-${NDK_VERSION}
ENV TOOLCHAIN=${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64
ENV PREFIX=/root/prefix
ENV PATH=$PATH:${TOOLCHAIN}/bin

RUN mkdir -p ${PREFIX} /root/src

RUN dnf install -y xz bzip2 zip unzip which wget git patch make cmake ninja-build gcc gcc-c++ glibc-devel \
        libstdc++-devel python3 && \
    dnf clean all

RUN curl -fsSLO https://dl.google.com/android/repository/android-ndk-${NDK_VERSION}-linux.zip && \
    unzip -q android-ndk-${NDK_VERSION}-linux.zip -d /root/Android && \
    rm android-ndk-${NDK_VERSION}-linux.zip

# ICU's cross build needs a build for the host first.
RUN cd /root/src && wget -q https://github.com/unicode-org/icu/archive/refs/tags/release-${LIBICU_VERSION}.zip && \
    unzip -q release-${LIBICU_VERSION}.zip && rm release-${LIBICU_VERSION}.zip
RUN mkdir -p /root/src/icu-host-build && cd $_ && \
    /root/src/icu-release-${LIBICU_VERSION}/icu4c/source/configure --disable-tests --disable-samples --disable-icuio \
        --disable-extras CC=gcc CXX=g++ && \
    make -j $(nproc)

ENV PKG_CONFIG_LIBDIR=${PREFIX}/lib/pkgconfig

# No LTO: it would move the dependencies' code generation into every link of libopenmw.so.
ENV CFLAGS="-fPIC -O3"
ENV CXXFLAGS="-fPIC -O3 -frtti -fexceptions"
ENV LDFLAGS="-fPIC -Wl,--undefined-version -fuse-ld=lld"

ENV COMMON_AUTOCONF_FLAGS="--enable-static \
  --disable-shared \
  --prefix=${PREFIX} \
  --host=${NDK_TRIPLET}${API} \
  CC=${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang \
  CXX=${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang++"

ENV COMMON_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=${ABI} \
  -DANDROID_PLATFORM=${API} \
  -DANDROID_STL=c++_shared \
  -DANDROID_CPP_FEATURES= \
  -DANDROID_ALLOW_UNDEFINED_VERSION_SCRIPT_SYMBOLS=ON \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_C_FLAGS=-I${PREFIX} \
  -DCMAKE_DEBUG_POSTFIX= \
  -DCMAKE_INSTALL_PREFIX=${PREFIX} \
  -DCMAKE_FIND_ROOT_PATH=${PREFIX} \
  -DHAVE_LD_VERSION_SCRIPT=OFF"

RUN mkdir -p /root/src/icu-${LIBICU_VERSION} && cd $_ && \
    /root/src/icu-release-${LIBICU_VERSION}/icu4c/source/configure ${COMMON_AUTOCONF_FLAGS} \
        --disable-tests --disable-samples --disable-icuio --disable-extras \
        --with-cross-build=/root/src/icu-host-build && \
    make -j $(nproc) check_PROGRAMS= bin_PROGRAMS= && \
    make install check_PROGRAMS= bin_PROGRAMS=

RUN wget -q https://github.com/libarchive/bzip2/archive/${BZIP2_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/bzip2-${BZIP2_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} && \
    make -j $(nproc) && make install

RUN wget -q https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/zlib-${ZLIB_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} && \
    make -j $(nproc) && make install

RUN wget -q https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEG_TURBO_VERSION}/libjpeg-turbo-${LIBJPEG_TURBO_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/libjpeg-turbo-${LIBJPEG_TURBO_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} -DENABLE_SHARED=false && \
    make -j $(nproc) && make install

RUN wget -q http://prdownloads.sourceforge.net/libpng/libpng-${LIBPNG_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/libpng-${LIBPNG_VERSION}/build && cd $_ && \
    /root/src/libpng-${LIBPNG_VERSION}/configure ${COMMON_AUTOCONF_FLAGS} && \
    make -j $(nproc) check_PROGRAMS= bin_PROGRAMS= && \
    make install check_PROGRAMS= bin_PROGRAMS=

RUN wget -q http://prdownloads.sourceforge.net/freetype/freetype-${FREETYPE2_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/freetype-${FREETYPE2_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE \
        -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE \
        -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE && \
    make -j $(nproc) && make install

# For collada-dom.
RUN wget -q https://github.com/GNOME/libxml2/archive/refs/tags/v${LIBXML2_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/libxml2-${LIBXML2_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DBUILD_SHARED_LIBS=OFF \
        -DLIBXML2_WITH_THREADS=ON \
        -DLIBXML2_WITH_CATALOG=OFF \
        -DLIBXML2_WITH_ICONV=OFF \
        -DLIBXML2_WITH_LZMA=OFF \
        -DLIBXML2_WITH_PROGRAMS=OFF \
        -DLIBXML2_WITH_PYTHON=OFF \
        -DLIBXML2_WITH_TESTS=OFF \
        -DLIBXML2_WITH_ZLIB=ON && \
    make -j $(nproc) && make install

# OpenAL plays through Oboe, which it builds from source.
RUN wget -q https://github.com/google/oboe/archive/refs/tags/${OBOE_VERSION}.tar.gz -O - | tar -xz -C /root/src
RUN wget -q https://github.com/kcat/openal-soft/archive/${OPENAL_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/openal-soft-${OPENAL_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DALSOFT_EXAMPLES=OFF \
        -DALSOFT_TESTS=OFF \
        -DALSOFT_UTILS=OFF \
        -DALSOFT_NO_CONFIG_UTIL=ON \
        -DALSOFT_BACKEND_OPENSL=OFF \
        -DALSOFT_BACKEND_OBOE=ON \
        -DOBOE_SOURCE=/root/src/oboe-${OBOE_VERSION} \
        -DALSOFT_BACKEND_WAVE=OFF && \
    make -j $(nproc) && make install

# collada-dom looks for boost_system and boost_regex libraries, which Boost's CMake build doesn't install, so they are
# put together from its object files.
RUN wget -q https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/boost-${BOOST_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} -DBOOST_INCLUDE_LIBRARIES="filesystem;program_options;iostreams;geometry" && \
    make -j $(nproc) && make install && \
    llvm-ar rc ${PREFIX}/lib/libboost_system.a $(find /root/src/boost-${BOOST_VERSION} -name "error_code.o") && \
    llvm-ar rc ${PREFIX}/lib/libboost_regex.a $(find /root/src/boost-${BOOST_VERSION} \( -name "posix_api.o" \
        -o -name "regex.o" -o -name "regex_debug.o" -o -name "static_mutex.o" -o -name "wide_posix_api.o" \)) && \
    llvm-ranlib ${PREFIX}/lib/libboost_{system,filesystem,program_options,iostreams,regex}.a

# Only the decoders that Morrowind's videos and sounds need.
RUN wget -q https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2 -O - | tar -xjf - -C /root/src && \
    cd /root/src/ffmpeg-${FFMPEG_VERSION} && \
    ./configure \
        --disable-asm \
        --disable-optimizations \
        --target-os=android \
        --enable-cross-compile \
        --cross-prefix=${TOOLCHAIN}/bin/llvm- \
        --cc=${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang \
        --arch=arm64 \
        --cpu=armv8-a \
        --prefix=${PREFIX} \
        --enable-version3 \
        --enable-pic \
        --disable-everything \
        --disable-doc \
        --disable-programs \
        --disable-autodetect \
        --disable-iconv \
        --enable-decoder=mp3 \
        --enable-demuxer=mp3 \
        --enable-decoder=bink \
        --enable-decoder=binkaudio_rdft \
        --enable-decoder=binkaudio_dct \
        --enable-demuxer=bink \
        --enable-demuxer=wav \
        --enable-decoder=pcm_* \
        --enable-decoder=vp8 \
        --enable-decoder=vp9 \
        --enable-decoder=opus \
        --enable-decoder=vorbis \
        --enable-demuxer=matroska \
        --enable-demuxer=ogg && \
    make -j $(nproc) && make install

# The app's Java code comes from the same SDL release. See ui-rework/android.md.
RUN wget -q https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/SDL2-${SDL2_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} -DSDL_STATIC=OFF -DHAVE_GCC_FVISIBILITY=OFF && \
    make -j $(nproc) && make install && \
    cp -rf /root/src/SDL2-${SDL2_VERSION}/include/* ${PREFIX}/include/

RUN wget -q https://github.com/bulletphysics/bullet3/archive/${BULLET_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/bullet3-${BULLET_VERSION}/build && cd $_ && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DBUILD_BULLET2_DEMOS=OFF \
        -DBUILD_CPU_DEMOS=OFF \
        -DBUILD_UNIT_TESTS=OFF \
        -DBUILD_EXTRAS=OFF \
        -DUSE_DOUBLE_PRECISION=ON \
        -DBULLET2_MULTITHREADING=ON && \
    make -j $(nproc) && make install

# NG-GL4ES implements desktop OpenGL on top of OpenGL ES.
RUN git clone https://github.com/sisah2/Ng-gl4es /root/src/ng-gl4es && \
    cd /root/src/ng-gl4es && git checkout ${GL4ES_VERSION} && git submodule update --init --recursive && \
    sed -i "s|#define DEFAULT_NGG_DIRECTORY_PATH .*|#define DEFAULT_NGG_DIRECTORY_PATH \"${GL4ES_DIRECTORY}\"|" src/gl/config.h && \
    grep -q "${GL4ES_DIRECTORY}" src/gl/config.h && \
    mkdir -p build && cd build && \
    cmake .. ${COMMON_CMAKE_ARGS} && \
    make -j $(nproc) && \
    cp *.so ${PREFIX}/lib/ && \
    cp -r /root/src/ng-gl4es/include ${PREFIX}/ && \
    cp -r /root/src/ng-gl4es/include ${PREFIX}/include/gl4es

RUN wget -q https://github.com/MyGUI/mygui/archive/MyGUI${MYGUI_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    cd /root/src/mygui-MyGUI${MYGUI_VERSION} && \
    sed -i 's/using unicode_char = uint32;/using unicode_char = char32_t;/g' MyGUIEngine/include/MyGUI_UString.h && \
    sed -i 's/using code_point = uint16;/using code_point = char16_t;/g' MyGUIEngine/include/MyGUI_UString.h && \
    mkdir -p build && cd build && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DMYGUI_RENDERSYSTEM=1 \
        -DMYGUI_BUILD_DEMOS=OFF \
        -DMYGUI_BUILD_TOOLS=OFF \
        -DMYGUI_BUILD_PLUGINS=OFF \
        -DMYGUI_DONT_USE_OBSOLETE=ON \
        -DMYGUI_STATIC=ON && \
    make -j $(nproc) && make install

RUN wget -q https://github.com/lz4/lz4/archive/v${LZ4_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    mkdir -p /root/src/lz4-${LZ4_VERSION}/build && cd $_ && \
    cmake ../build/cmake ${COMMON_CMAKE_ARGS} -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF && \
    make -j $(nproc) && make install

RUN wget -q https://github.com/LuaJIT/LuaJIT/archive/${LUAJIT_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    cd /root/src/LuaJIT-${LUAJIT_VERSION} && \
    LUAJIT_FLAGS="HOST_CC=gcc CFLAGS= PREFIX=${PREFIX} CROSS=${TOOLCHAIN}/bin/llvm- \
        STATIC_CC=${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang TARGET_LD=${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang" && \
    make amalg ${LUAJIT_FLAGS} TARGET_CFLAGS="${CFLAGS}" \
        DYNAMIC_CC="${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang -fPIC" && \
    make install ${LUAJIT_FLAGS} TARGET_CFLAGS="${CFLAGS}" \
        DYNAMIC_CC="${TOOLCHAIN}/bin/${NDK_TRIPLET}${API}-clang -fPIC" && \
    rm ${PREFIX}/lib/libluajit*.so*

# OpenMW loads COLLADA models through OSG's dae plugin. The sed commands are for Boost 1.85 and later.
RUN wget -q https://github.com/rdiankov/collada-dom/archive/v${COLLADA_DOM_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    cd /root/src/collada-dom-${COLLADA_DOM_VERSION} && \
    sed -i 's|#include <boost/filesystem/convenience.hpp>|#include <boost/filesystem.hpp>|g' dom/include/dae.h && \
    sed -i 's|#include <boost/filesystem/convenience.hpp>|#include <boost/filesystem.hpp>|g' dom/src/dae/daeUtils.cpp && \
    sed -i 's|std::string dir = archivePath.branch_path().string();|std::string dir = archivePath.parent_path().string();|g' dom/src/dae/daeZAEUncompressHandler.cpp && \
    mkdir -p build && cd build && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DBoost_USE_STATIC_LIBS=ON \
        -DBoost_USE_STATIC_RUNTIME=ON \
        -DBoost_NO_SYSTEM_PATHS=ON \
        -DBoost_INCLUDE_DIR=${PREFIX}/include \
        -DCMAKE_CXX_FLAGS=-Dauto_ptr=unique_ptr\ "${CXXFLAGS}" && \
    make -j $(nproc) && make install

COPY patches/osg /root/patches/osg
RUN wget -q https://github.com/OpenMW/osg/archive/${OSG_VERSION}.tar.gz -O - | tar -xz -C /root/src && \
    cd /root/src/osg-${OSG_VERSION} && \
    while read -r p; do patch -p1 -t -N < /root/patches/osg/$p || exit 1; done < /root/patches/osg/series && \
    mkdir -p build && cd build && \
    cmake .. ${COMMON_CMAKE_ARGS} \
        -DOPENGL_PROFILE=GL2 \
        -DDYNAMIC_OPENTHREADS=OFF \
        -DDYNAMIC_OPENSCENEGRAPH=OFF \
        -DBUILD_OSG_PLUGIN_OSG=ON \
        -DBUILD_OSG_PLUGIN_DAE=ON \
        -DBUILD_OSG_PLUGIN_DDS=ON \
        -DBUILD_OSG_PLUGIN_TGA=ON \
        -DBUILD_OSG_PLUGIN_BMP=ON \
        -DBUILD_OSG_PLUGIN_JPEG=ON \
        -DBUILD_OSG_PLUGIN_PNG=ON \
        -DBUILD_OSG_PLUGIN_KTX=ON \
        -DBUILD_OSG_PLUGIN_FREETYPE=ON \
        -DOSG_CPP_EXCEPTIONS_AVAILABLE=TRUE \
        -DJPEG_INCLUDE_DIR=${PREFIX}/include/ \
        -DPNG_INCLUDE_DIR=${PREFIX}/include/ \
        -DCOLLADA_INCLUDE_DIR=${PREFIX}/include/collada-dom2.5 \
        -DCOLLADA_DIR=${PREFIX}/include/collada-dom2.5/1.4 \
        -DOSG_GL1_AVAILABLE=ON \
        -DOSG_GL2_AVAILABLE=ON \
        -DOSG_GL3_AVAILABLE=OFF \
        -DOSG_GLES1_AVAILABLE=OFF \
        -DOSG_GLES3_AVAILABLE=OFF \
        -DOSG_GL_LIBRARY_STATIC=OFF \
        -DOSG_GL_DISPLAYLISTS_AVAILABLE=ON \
        -DOSG_GL_MATRICES_AVAILABLE=ON \
        -DOSG_GL_VERTEX_FUNCS_AVAILABLE=ON \
        -DOSG_GL_VERTEX_ARRAY_FUNCS_AVAILABLE=ON \
        -DOSG_GL_FIXED_FUNCTION_AVAILABLE=ON \
        -DBUILD_OSG_APPLICATIONS=OFF \
        -DBUILD_OSG_PLUGINS_BY_DEFAULT=OFF \
        -DBUILD_OSG_DEPRECATED_SERIALIZERS=OFF \
        -DOSG_FIND_3RD_PARTY_DEPS=OFF \
        -DOPENGL_INCLUDE_DIR=${PREFIX}/include/gl4es/ \
        -DOPENGL_gl_LIBRARY=${PREFIX}/lib/libng_gl4es.so \
        -DCMAKE_CXX_FLAGS=-Dauto_ptr=unique_ptr\ -I${PREFIX}/include/freetype2/\ "${CXXFLAGS}" && \
    make -j $(nproc) && make install

# For android/container-build.sh, which builds OpenMW itself.
RUN dnf install -y ccache && dnf clean all
