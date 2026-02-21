#pragma once

#include <string>
#include <vector>
#include <memory>

#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>

namespace JSBSim {
    class FGFDMExec;
    class FGPropertyManager;
}

namespace flightgear {
namespace addons {

class FDMManager
{
public:
    FDMManager(const std::string& addonId,
               const SGPath& addonPath,
               const std::string& model,
               const std::string& root,
               bool disableDefault,
               std::vector<std::string> publishList,
               const std::string& mode,
               const std::string& pathOverride,
               bool debug);

    void init();
    void shutdown();
    void update(double dt);

    double getJSBSimProperty(const std::string& path) const;
    void   setJSBSimProperty(const std::string& path, double value);
    void   step(double dt);

private:
    std::string _addonId;
    SGPath      _addonPath;
    std::string _fdmModel;
    std::string _fdmRoot;
    bool        _disableDefault;
    std::vector<std::string> _publishList;
    std::string _mode;
    std::string _fdmPath;
    bool        _debug = false;

    std::unique_ptr<JSBSim::FGFDMExec> _jsb;
};

} // namespace addons
} // namespace flightgear
