#!/usr/bin/env bash
set -euo pipefail

find include src apps tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format -i

