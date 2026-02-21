#include <filesystem>
namespace fs = std::filesystem;

#include "FDMManager.hxx"
#include "PropertyTreeScanner.hxx"
#include "PropertyTreeBuilder.hxx"
#include "SetFileParser.hxx"
#include "PropertyListScanner.hxx"

#include <simgear/debug/logstream.hxx>
#include <Main/fg_props.hxx>
#include <Main/fg_init.hxx>
#include <simgear/misc/sg_path.hxx>
#include <fstream>
#include <algorithm>

#include <FDM/JSBSim/FGFDMExec.h>

using namespace JSBSim;

namespace flightgear {
namespace addons {

FDMManager::FDMManager(const std::string& addonId,
                       const SGPath& addonPath,
                       const std::string& model,
                       const std::string& root,
                       bool disableDefault,
                       std::vector<std::string> publishList,
                       const std::string& mode,
                       const std::string& pathOverride,
                       bool debug)
    : _addonId(addonId),
      _addonPath(addonPath),
      _fdmModel(model),
      _fdmRoot(root),
      _disableDefault(disableDefault),
      _publishList(std::move(publishList)),
      _mode(mode),
      _fdmPath(pathOverride),
      _debug(debug)

{
}

double FDMManager::getJSBSimProperty(const std::string& path) const
{
    return _jsb->GetPropertyValue(path);
}

void FDMManager::setJSBSimProperty(const std::string& path, double value)
{
    _jsb->SetPropertyValue(path, value);
}

void FDMManager::step(double dt)
{
    _jsb->Run();
}

// ------------------------------------------------------------
// Detect aero block name from the model XML
// Returns:
//   ""      → no aero block (benchmark mode)
//   "aero"  → unnamed aero block
//   "foo"   → <aerodynamics name="foo">
// ------------------------------------------------------------
static std::string detectAeroName(const std::string& modelPath)
{
    std::ifstream file(modelPath);
    if (!file.is_open())
        return "";   // benchmark mode fallback

    std::string line;
    while (std::getline(file, line)) {

        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        size_t pos = lower.find("<aerodynamics");
        if (pos == std::string::npos)
            continue;

        // Named aero block
        size_t namePos = lower.find("name=", pos);
        if (namePos != std::string::npos) {
            size_t q1 = lower.find('"', namePos);
            size_t q2 = lower.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                return lower.substr(q1 + 1, q2 - q1 - 1);  // "foo"
            }
        }

        // Unnamed aero block
        return "aero";
    }

    // No aero block → benchmark mode
    return "";
}

// ------------------------------------------------------------
// FDMManager::init()
// ------------------------------------------------------------
void FDMManager::init()
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

    _jsb = std::make_unique<FGFDMExec>(nullptr, nullptr);

    // ------------------------------------------------------------
    // Combined benchmark/aircraft mode logic
    // ------------------------------------------------------------
    SGPath rootDir;
    SGPath aircraftPath;
    SGPath loadModelPath = rootDir;
    std::string modelFileName = _fdmModel + ".xml";
    std::string modelName;

    if (_mode == "benchmark") {
        rootDir      = _addonPath / "JSBSim";
        aircraftPath = rootDir / _fdmModel;
        modelName    = _fdmModel;   // "benchmark"
        loadModelPath = modelName;
    }
    else { // aircraft mode
        SGPath aircraftBase = !_fdmPath.empty()
            ? SGPath(_fdmPath)
            : SGPath(fgGetString("/sim/aircraft-dir"));

        rootDir      = SGPath(_fdmPath);   // /.../J3Cub
        aircraftPath = rootDir;            // DO NOT append model name or filename
        modelFileName = _fdmModel + ".xml";  // "J3Cub.xml"
        modelName     = _fdmModel;           // "J3Cub"
        loadModelPath = rootDir;
        
        if (modelName.empty())
            modelName = _fdmModel;  // fallback to model file
    }

    // ------------------------------------------------------------
    // Build remaining paths
    // ------------------------------------------------------------
    SGPath fullModelPath = aircraftPath / modelFileName;
    SGPath enginePath    = aircraftPath / "Engines";
    SGPath systemsPath   = aircraftPath / "Systems";

    // ------------------------------------------------------------
    // Debug output
    // ------------------------------------------------------------
    if (_debug) {
        SG_LOG(SG_GENERAL, SG_ALERT, "=== JSBSim PATH DEBUG START ===");
        SG_LOG(SG_GENERAL, SG_ALERT, "addonPath                         = " << _addonPath.str());
        SG_LOG(SG_GENERAL, SG_ALERT, "rootDir                           = " << rootDir.str());
        SG_LOG(SG_GENERAL, SG_ALERT, "aircraftPath                      = " << aircraftPath.str());
        SG_LOG(SG_GENERAL, SG_ALERT, "modelFileName                     = " << modelFileName);
        SG_LOG(SG_GENERAL, SG_ALERT, "fullModelPath                     = " << fullModelPath.str());
        SG_LOG(SG_GENERAL, SG_ALERT, "aeroName/modelName/_fdmMmodeel    = '" << modelName << "'");
        SG_LOG(SG_GENERAL, SG_ALERT, "enginePath                        = " << enginePath.str());
        SG_LOG(SG_GENERAL, SG_ALERT, "systemsPath                       = " << systemsPath.str());
        SG_LOG(SG_GENERAL, SG_ALERT, "=== JSBSim PATH DEBUG END ===");
    }
    // ------------------------------------------------------------
    // Set JSBSim search paths
    // ------------------------------------------------------------
    _jsb->SetRootDir(rootDir.str());
    _jsb->SetAircraftPath(aircraftPath.str());

    // ------------------------------------------------------------
    // 1. Scan JSBSim FDM XML (JSBSim-side properties)
    // ------------------------------------------------------------
    PropertyTreeScanner scanner;
    scanner.scanJSBSimFile(fullModelPath.str());

    // ------------------------------------------------------------
    // 2. Determine addon aircraft root and -set.xml path
    // ------------------------------------------------------------
    SGPath addonAircraftRoot = rootDir;   // rootDir already points to the aircraft directory
    SGPath setFile = addonAircraftRoot / (modelName + "-set.xml");

    // ------------------------------------------------------------
    // 3. Recursively parse -set.xml and all referenced system files
    // ------------------------------------------------------------
    SetFileParser setParser(addonAircraftRoot.str());
    setParser.parse(setFile.str());

    // ------------------------------------------------------------
    // 4. Scan all discovered FG-side XML files
    // ------------------------------------------------------------
    for (const auto& f : setParser.getAllFiles()) {
        scanner.scanFGFile(f);
    }

    // ------------------------------------------------------------
    // 5. Scan inline <PropertyList> blocks inside the -set.xml
    // ------------------------------------------------------------
    PropertyListScanner plistScanner;
    plistScanner.scan(setFile.str());

    for (const auto& p : plistScanner.getProperties()) {
        scanner.addFGProperty(p);

        // NEW: If path starts with "fdm/jsbsim/", also add FG root version
        if (p.rfind("fdm/jsbsim/", 0) == 0) {
            std::string rootPath = p.substr(strlen("fdm/jsbsim/"));
            scanner.addFGProperty(rootPath);

            SG_LOG(SG_GENERAL, SG_ALERT,
               "PropertyListScanner: mapped JSBSim inline property to FG root: "
               << rootPath);
        }
    }

    // ------------------------------------------------------------
    // 6. Build property trees
    // ------------------------------------------------------------

    // JSBSim properties go under /addon/fdm/jsbsim
    PropertyTreeBuilder jsbBuilder("/addon/fdm/jsbsim");
    jsbBuilder.build(scanner.getJSBSimProperties());

    // FG-side properties go at the root
    PropertyTreeBuilder fgBuilder("/");
    fgBuilder.build(scanner.getFGProperties());

    // ------------------------------------------------------------
    // Load model using JSBSim 5 arg API
    // ------------------------------------------------------------
    if (!_jsb->LoadModel(loadModelPath,
                         enginePath.str(),
                         systemsPath.str(),
                         modelName,
                         false))
        {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "Addon JSBSim: LoadModel() failed for " << aircraftPath.str());
            return;
        }

    SG_LOG(SG_GENERAL, SG_ALERT, "JSBSim: LoadModel() succeeded");

    if (!_jsb->RunIC()) {
        SG_LOG(SG_GENERAL, SG_ALERT, "JSBSim: RunIC() failed");
        return;
    }

    SG_LOG(SG_GENERAL, SG_INFO, "JSBSim addon FDM initialized successfully");

}

void FDMManager::shutdown()
{
    _jsb.reset();
}

} // namespace addons
} // namespace flightgear
