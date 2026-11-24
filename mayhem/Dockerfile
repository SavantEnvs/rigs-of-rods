# syntax=docker/dockerfile:1.7
#
# Commit image for Rigs of Rods. The game itself links OGRE/MyGUI/OIS/OpenAL and a render system;
# the fuzz harnesses link only the file-format parsers (source/main/resources/*, utils/) against
# Debian's OGRE 1.12 OgreMain + headers, plus AngelScript (built from a pinned SDK zip, since
# Debian ships no libangelscript-dev and RoR's RefCountingObject.h includes <angelscript.h>).
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libogre-1.12-dev libmygui-dev libois-dev libfmt-dev rapidjson-dev unzip cmake \
 && rm -rf /var/lib/apt/lists/*

# AngelScript 2.38.0 (the version upstream's conanfile.py requires), static, uninstrumented.
ARG AS_VER=2.38.0
ARG AS_SHA256=b33b5dbcda10317ef67d628353d83246984ce6fcac102d4dc2aed121eba52e6f
RUN d=/opt/toolchains/deps/angelscript-${AS_VER} && mkdir -p "$d/src" "$d/include" "$d/lib" /tmp/dep-build \
 && curl -fsSL -o /tmp/as.zip https://www.angelcode.com/angelscript/sdk/files/angelscript_${AS_VER}.zip \
 && echo "${AS_SHA256}  /tmp/as.zip" | sha256sum -c - \
 && unzip -q /tmp/as.zip -d /tmp/as && cp -a /tmp/as/sdk/. "$d/src/" \
 && cd /tmp/dep-build && for f in "$d"/src/angelscript/source/*.cpp; do \
        clang++ -O2 -g -gdwarf-3 -fPIC -std=c++17 -c "$f" -o "$(basename "$f" .cpp).o" || exit 1; done \
 && for f in "$d"/src/angelscript/source/*.S; do [ -e "$f" ] || continue; \
        clang -c "$f" -o "$(basename "$f" .S).S.o" || exit 1; done \
 && ar rcs "$d/lib/libangelscript.a" ./*.o \
 && cp "$d/src/angelscript/include/angelscript.h" "$d/include/" \
 && cd / && rm -rf /tmp/dep-build /tmp/as /tmp/as.zip && chmod -R a+rX "$d"
USER mayhem

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
