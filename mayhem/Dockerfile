# syntax=docker/dockerfile:1.7
#
# Commit image: FreeType built on the org C/C++ base, with the optional zlib/bzip2/libpng/brotli
# back-ends so the compressed-stream, WOFF/WOFF2 and PNG-glyph code paths are part of the fuzzed surface.
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
        zlib1g-dev libbz2-dev libpng-dev libbrotli-dev libharfbuzz-dev \
 && rm -rf /var/lib/apt/lists/*
# tests/issue-1063's font (pinned in tests/scripts/download-test-fonts.py, verified here by sha256), cached
# so build.sh/test.sh stay offline (SPEC 6.2 item 19). HarfBuzz is linked into the oracle build only, for
# tests/issue-1441.
RUN d=/opt/toolchains/deps/as-i-lay-dying-6622147 && mkdir -p "$d" \
 && curl -fsSL -o /tmp/aild.zip https://github.com/python-pillow/Pillow/files/6622147/As.I.Lay.Dying.zip \
 && echo "09fceee7c335975a454500b000f75ee7f7b5c4f14b2dc241d6416f77967e9d1b  /tmp/aild.zip" | sha256sum -c - \
 && unzip -p /tmp/aild.zip "As I Lay Dying.ttf" > "$d/As.I.Lay.Dying.ttf" \
 && echo "ef146bbc2673b38715afcc70dca484cc64457438da8627a27cf95680e982f0a6  $d/As.I.Lay.Dying.ttf" | sha256sum -c - \
 && rm /tmp/aild.zip && chmod -R a+rX /opt/toolchains/deps
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
