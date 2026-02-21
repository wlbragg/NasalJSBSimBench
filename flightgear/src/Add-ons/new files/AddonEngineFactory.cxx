#include "AddonEngineFactory.hxx"
#include "AddonEngine.hxx"
#include "AddonJSBSimEngine.hxx"

namespace flightgear {
namespace addons {

std::unique_ptr<AddonEngine>
AddonEngineFactory::create(const std::string& name,
                           const std::string& addonId,
                           const std::string& addonPath)
{
    if (name == "AddonJSBSimEngine") {
        return std::make_unique<AddonJSBSimEngine>(addonId, addonPath);
    }

    return nullptr;
}

} // namespace addons
} // namespace flightgear
