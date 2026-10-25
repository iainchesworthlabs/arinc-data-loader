# SPDX-License-Identifier: GPL-3.0-or-later
# The image ClusterFuzzLite builds the libFuzzer harnesses in (docs/fuzzing.md). The base image is
# pinned by digest; Dependabot's docker ecosystem moves it.
FROM gcr.io/oss-fuzz-base/base-builder@sha256:1f9cf74bc3d7029b528494542f9d9851b256dceed38d3f420989434dc5ae0760

COPY . $SRC/arinc-data-loader
COPY .clusterfuzzlite/build.sh $SRC/build.sh
WORKDIR $SRC/arinc-data-loader
