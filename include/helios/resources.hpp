#pragma once

namespace helios {

using ResourceAmount = int;

struct Resources {
  ResourceAmount gpu_count;
  ResourceAmount cpu_count;
  ResourceAmount memory_mb;

  bool operator==(const Resources&) const = default;
};

void validate_resources(const Resources& resources);

}  // namespace helios
