#include "PropertyTreeScanner.hxx"
#include <simgear/debug/logstream.hxx>
#include <simgear/xml/easyxml.hxx>
#include <simgear/misc/sg_path.hxx>
#include <cstring>

PropertyTreeScanner::PropertyTreeScanner()
{
}

void PropertyTreeScanner::scanJSBSimFile(const std::string& path)
{
    _currentTarget = Target::JSBSIM;

    SG_LOG(SG_GENERAL, SG_ALERT, "PropertyTreeScanner: scanning JSBSim file: " << path);

    try {
        readXML(SGPath(path), *this);
    }
    catch (const sg_exception& e) {
        SG_LOG(SG_GENERAL, SG_ALERT, "PropertyTreeScanner JSBSim scan failed: " << e.getMessage());
    }

    _currentTarget = Target::NONE;
}

void PropertyTreeScanner::scanFGFile(const std::string& path)
{
    _currentTarget = Target::FG;

    SG_LOG(SG_GENERAL, SG_ALERT, "PropertyTreeScanner: scanning FG file: " << path);

    try {
        readXML(SGPath(path), *this);
    }
    catch (const sg_exception& e) {
        SG_LOG(SG_GENERAL, SG_ALERT, "PropertyTreeScanner FG scan failed: " << e.getMessage());
    }

    _currentTarget = Target::NONE;
}

void PropertyTreeScanner::addFGProperty(const std::string& p)
{
    _fgProperties.insert(p);
}

void PropertyTreeScanner::startElement(const char* name, const XMLAttributes& atts)
{
    // Nothing special to do here for now.
    // Property extraction happens in data().
}

void PropertyTreeScanner::data(const char* s, int length)
{
    if (_currentTarget == Target::NONE)
        return;

    std::string text(s, length);
    extractProperties(text);
}

void PropertyTreeScanner::extractProperties(const std::string& text)
{
    std::string trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

    if (trimmed.empty())
        return;

    // Must contain at least one slash
    if (trimmed.find('/') == std::string::npos)
        return;

    // Must not contain spaces
    if (trimmed.find(' ') != std::string::npos)
        return;

    // Must not contain operators
    if (trimmed == "eq" || trimmed == "gt" || trimmed == "lt" ||
        trimmed == "and" || trimmed == "or")
        return;

    // Now it's a valid property path
    if (_currentTarget == Target::JSBSIM)
        _jsbProperties.insert(trimmed);
    else
        _fgProperties.insert(trimmed);

    SG_LOG(SG_GENERAL, SG_ALERT, "PropertyTreeScanner: property: " << trimmed);
}
