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
bool less_equal(const Resources& lhs, const Resources& rhs);
bool fits(const Resources& available, const Resources& requested);
Resources allocate(const Resources& available, const Resources& requested);
Resources release(const Resources& available, const Resources& total, const Resources& released);

} // namespace helios
