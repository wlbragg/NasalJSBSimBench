#include "AddonJSBSimEngine.hxx"

#include <simgear/debug/logstream.hxx>
#include <Main/fg_props.hxx>
#include <Main/fg_init.hxx>

namespace flightgear {
namespace addons {

AddonJSBSimEngine::AddonJSBSimEngine(const std::string& addonId,
                                     const SGPath& addonPath)
    : _addonId(addonId),
      _addonPath(addonPath)
{
    _disableDefault = false;
    _fdmModel.clear();
    _fdmRoot.clear();
    _fdmPath.clear();
    _mode.clear();
    _debug = false;
}

void AddonJSBSimEngine::setFDMConfig(bool disableDefault,
                                     const std::string& model,
                                     const std::string& root)
{
    _disableDefault = disableDefault;
    _fdmModel       = model;
    _fdmRoot        = root;
}

void AddonJSBSimEngine::configureFromProperties()
{
    SGPropertyNode* jsbNode =
        fgGetNode("/addon/fdm/jsbsim", false);

    if (!jsbNode) {
        SG_LOG(SG_GENERAL, SG_ALERT,
               "AddonJSBSimEngine: Missing /addon/fdm/jsbsim node");
        return;
    }

    // ---------------------------------------------------------
    // debug mode
    // ---------------------------------------------------------
    if (auto* dbgNode = jsbNode->getChild("debug"))
        _debug = dbgNode->getBoolValue(false);

    const std::string basePath = "/addon/fdm/jsbsim";

    if (_debug) {
        SGPropertyNode* baseNode = fgGetNode(basePath.c_str(), false);
        if (!baseNode) {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "AddonJSBSimEngine: base path not found: " << basePath);
            return;
        }
    }

    if (_debug) {
        SG_LOG(SG_GENERAL, SG_ALERT,
               "AddonJSBSimEngine: mode=" << _mode
               << " model=" << _fdmModel
               << " root=" << _fdmRoot
               << " publish-count=" << _publishList.size());
        for (const auto& p : _publishList) {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "AddonJSBSimEngine: publish entry=" << p);
        }
    }

    // ---------------------------------------------------------
    // fdm-control (mode + path)
    // ---------------------------------------------------------
    if (auto* ctrl = jsbNode->getChild("fdm-control")) {

        if (auto* modeNode = ctrl->getChild("mode"))
            _mode = modeNode->getStringValue();

        if (auto* pathNode = ctrl->getChild("path"))
            _fdmPath = pathNode->getStringValue();
    }

    // ---------------------------------------------------------
    // publish list
    // ---------------------------------------------------------
    _publishList.clear();

    if (auto* pubNode = jsbNode->getChild("publish")) {
        const int n = pubNode->nChildren();
        for (int i = 0; i < n; i++) {
            SGPropertyNode* child = pubNode->getChild(i);
            if (!child) continue;

            if (child->getNameString() == "property") {
                std::string p = child->getStringValue();
                if (!p.empty())
                    _publishList.push_back(p);
            }
        }
    }
}

void AddonJSBSimEngine::init()
{
    configureFromProperties();

    // Always-on lifecycle log
    SG_LOG(SG_GENERAL, SG_ALERT,
           "AddonJSBSimEngine: init addon=" << _addonId
           << " model=" << _fdmModel
           << " root=" << _fdmRoot
           << " mode=" << _mode
           << " disable-default=" << _disableDefault
           << " publish-count=" << _publishList.size());

    // ---------------------------------------------------------
    // Debug-only property tree dumps
    // ---------------------------------------------------------
    if (_debug) {
        SG_LOG(SG_GENERAL, SG_ALERT, "UPDATE CALLED");
        SG_LOG(SG_GENERAL, SG_ALERT,
               "PUBLISH LIST SIZE = " << _publishList.size());
        SG_LOG(SG_GENERAL, SG_ALERT,
               "DUMP: Listing /addon subtree");

        SGPropertyNode* root = fgGetNode("/addon", false);
        if (root) {
            for (int i = 0; i < root->nChildren(); i++) {
                SGPropertyNode* child = root->getChild(i);
                SG_LOG(SG_GENERAL, SG_ALERT,
                       "DUMP: child node = " << child->getNameString());
            }
        } else {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "DUMP: /addon does NOT exist");
        }

        SG_LOG(SG_GENERAL, SG_ALERT,
               "DUMP: Listing /sim subtree");

        SGPropertyNode* simRoot = fgGetNode("/sim", false);
        if (simRoot) {
            for (int i = 0; i < simRoot->nChildren(); i++) {
                SGPropertyNode* child = simRoot->getChild(i);
                SG_LOG(SG_GENERAL, SG_ALERT,
                       "DUMP: /sim child = " << child->getNameString());
            }
        } else {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "DUMP: /sim does NOT exist");
        }
    }

    // Always-on lifecycle log
    SG_LOG(SG_GENERAL, SG_INFO,
           "AddonJSBSimEngine: init addon=" << _addonId
           << " model=" << _fdmModel
           << " root=" << _fdmRoot
           << " mode=" << _mode
           << " disable-default=" << _disableDefault
           << " publish-count=" << _publishList.size());

    // ---------------------------------------------------------
    // Create FDMManager
    // ---------------------------------------------------------
    _fdmManager = std::make_unique<FDMManager>(
        _addonId,
        _addonPath,
        _fdmModel,
        _fdmRoot,
        _disableDefault,
        _publishList,
        _mode,
        _fdmPath,
        _debug
    );

    _fdmManager->init();
}

void AddonJSBSimEngine::update(double dt)
{
    if (!_fdmManager)
        return;

    // ------------------------------------------------------------
    // 1. FG → JSBSim sync (inputs)
    // ------------------------------------------------------------
    for (const auto& jsbPath : _publishList)
    {
        // Inputs = anything NOT starting with "fcs/"
        if (jsbPath.rfind("fcs/", 0) != 0)
        {
            std::string fgPath = "/addons/" + _addonId +
                                 "/fdm/jsbsim/" + jsbPath;

            double v = fgGetNode(fgPath, true)->getDoubleValue();
            _fdmManager->setJSBSimProperty(jsbPath, v);
        }
    }

    // ------------------------------------------------------------
    // 2. Run JSBSim
    // ------------------------------------------------------------
    _fdmManager->step(dt);

    // ------------------------------------------------------------
    // 3. JSBSim → FG publish (outputs)
    // ------------------------------------------------------------
    for (const auto& jsbPath : _publishList)
    {
        std::string fgPath = "/addons/" + _addonId +
                             "/fdm/jsbsim/" + jsbPath;

        double v = _fdmManager->getJSBSimProperty(jsbPath);
        fgGetNode(fgPath, true)->setDoubleValue(v);
    }
}

void AddonJSBSimEngine::shutdown()
{
    // No special shutdown needed
}

} // namespace addons
} // namespace flightgear
