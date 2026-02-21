#pragma once

#include <FDM/JSBSim/FGFDMExec.h>
#include <simgear/props/props.hxx>
#include <simgear/misc/sg_path.hxx>

#include "AddonEngine.hxx"
#include "FDMManager.hxx"

namespace flightgear {
namespace addons {

class AddonJSBSimEngine : public AddonEngine
{
public:
    AddonJSBSimEngine(const std::string& addonId,
                      const SGPath& addonPath);
    virtual ~AddonJSBSimEngine() = default;
    std::string getName() const override { return "jsbsim"; }
    void init() override;
    void shutdown() override;
    void update(double dt) override;
    void setFDMConfig(bool disableDefault,
        const std::string& model,
        const std::string& root);

private:
    void configureFromProperties();

    std::string _addonId;
    SGPath _addonPath;

    std::string _fdmModel = "";
    std::string _fdmRoot  = "";
    bool _disableDefault  = false;
    std::vector<std::string> _publishList;
    std::string _mode;
    std::string _fdmPath;
    bool _debug = false;

    std::unique_ptr<FDMManager> _fdmManager;
};

} // namespace addons
} // namespace flightgear
