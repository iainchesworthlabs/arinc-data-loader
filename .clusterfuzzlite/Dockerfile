# SPDX-License-Identifier: GPL-3.0-or-later
# The image ClusterFuzzLite builds the libFuzzer harnesses in (docs/fuzzing.md). The base image is
# pinned by digest; Dependabot's docker ecosystem moves it.
FROM gcr.io/oss-fuzz-base/base-builder@sha256:8954f7ece3f9e0731a748ffd91ee966bc032b3340725e773aef3d7e82eb5d8b6

COPY . $SRC/arinc-data-loader
COPY .clusterfuzzlite/build.sh $SRC/build.sh
WORKDIR $SRC/arinc-data-loader
