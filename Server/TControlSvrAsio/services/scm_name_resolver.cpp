#include "scm_name_resolver.h"

namespace tcontrolsvr {

std::string RenderScmName(const std::string& tmpl, const ServiceInstance& svc)
{
    std::string out;
    out.reserve(tmpl.size() + 8);
    for (std::size_t i = 0; i < tmpl.size(); )
    {
        if (tmpl[i] == '{')
        {
            const auto close = tmpl.find('}', i);
            if (close == std::string::npos) { out.push_back(tmpl[i++]); continue; }
            const auto key = tmpl.substr(i + 1, close - i - 1);
            if      (key == "type_name") out += svc.name;
            else if (key == "type")      out += std::to_string(svc.type_id);
            else if (key == "group")     out += std::to_string(svc.group_id);
            else if (key == "server")    out += std::to_string(svc.server_id);
            else if (key == "machine")   out += std::to_string(svc.machine_id);
            // Unknown placeholders are silently dropped — the closing
            // brace is consumed so the template stays well-formed.
            i = close + 1;
        }
        else
        {
            out.push_back(tmpl[i++]);
        }
    }
    return out;
}

std::string ResolveScmName(
    const ServiceInstance& svc,
    const std::string& template_str,
    const std::unordered_map<std::uint32_t, std::string>& overrides)
{
    auto it = overrides.find(svc.service_id);
    if (it != overrides.end() && !it->second.empty())
        return it->second;
    return RenderScmName(template_str, svc);
}

} // namespace tcontrolsvr
