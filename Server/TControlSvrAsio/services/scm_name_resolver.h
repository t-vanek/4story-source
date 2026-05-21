#pragma once

// Shared `ServiceInstance → SCM/unit name` rendering used by both
// WindowsScmServiceController and SystemdServiceController. Lives in
// one place so the two backends can't drift on the template grammar.
//
// Template grammar mirrors the legacy installer's m_szServiceName
// convention:
//
//   {type_name}   — ServiceInstance::name (e.g. "MapSvr")
//   {type}        — type_id as decimal
//   {group}       — group_id as decimal
//   {server}      — server_id as decimal
//   {machine}     — machine_id as decimal
//
// Anything outside `{...}` is copied verbatim. Unknown placeholders
// are stripped (the closing brace is consumed). Default template
// "{type_name}-{group}-{server}" renders a MapSvr in group 1 / server
// 3 as "MapSvr-1-3", matching what InstallShield writes for legacy
// deployments. Per-service overrides (e.g. when the SCM service name
// diverges from the binary name) are applied BEFORE this template —
// see ServiceControllerFactoryConfig::overrides.

#include "service_inventory.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tcontrolsvr {

// Render `tmpl` against `svc`. Pure helper — no overrides considered.
std::string RenderScmName(const std::string& tmpl,
                          const ServiceInstance& svc);

// Resolve the SCM/systemd name for `svc`: per-service override if
// present in the map, else rendered template. Operators set
// overrides via the [cluster.scm.overrides] TOML block for deploys
// where the SCM service name doesn't follow the template (legacy
// hand-installed services, vendor-renamed services, etc.).
std::string ResolveScmName(
    const ServiceInstance& svc,
    const std::string& template_str,
    const std::unordered_map<std::uint32_t, std::string>& overrides);

} // namespace tcontrolsvr
