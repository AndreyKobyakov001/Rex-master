
#include "ExtractionConfig.h"
#include "RexArgs.h"
#include "../Linker/LenDataStr.h"
#include <boost/algorithm/string.hpp>
#include <iostream>
using namespace std;
ExtractionConfig::ExtractionConfig(){}
ExtractionConfig::~ExtractionConfig()
{
    for(auto it : options)
    {
        delete it.second;
    }
    
}
ConfigOption::~ConfigOption(){}
void ExtractionConfig::addOption(string option, ConfigOption *value)
{
    options[option] = value;
}
unsigned int ExtractionConfig::size() const
{
    return options.size();
}
ExtractionConfig::iterator ExtractionConfig::begin()
{
    return options.begin();
}
ExtractionConfig::iterator ExtractionConfig::end()
{
    return options.end();
}
ExtractionConfig::const_iterator ExtractionConfig::begin() const
{
    return options.begin();
}
ExtractionConfig::const_iterator ExtractionConfig::end() const
{
    return options.end();
}
ExtractionConfig::iterator ExtractionConfig::find(string key)
{
    return options.find(key);
}
ExtractionConfig::const_iterator ExtractionConfig::find(string key) const
{
    return options.find(key);
}
bool ExtractionConfig::hasKey(string key) const
{
    return find(key) != end();
}
ConfigOption& ExtractionConfig::get(string key)
{
    ConfigOption *config = options.at(key);
    return *config;
}
const ConfigOption& ExtractionConfig::get(string key) const
{
    ConfigOption *config = options.at(key);
    return *config;
}
bool ExtractionConfig::operator==(const ExtractionConfig &other) const
{
    if (options.size() == other.size())
    {
        bool equal = true;
        for(auto optThis = begin(); equal && optThis != end(); optThis++)
        {
            if(other.hasKey(optThis->first))
            {
                const ConfigOption &lhs = *optThis->second;
                const ConfigOption &rhs = other.get(optThis->first);
                equal = lhs == rhs;
            }
            else
            {
                equal = false;
            }
            
        }
        return equal;
    }
    return false;
}
Json::Value ExtractionConfig::readConfigFile(std::istream &in) {
    Json::Value extractionConfigJSON;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    if (!(in.good() && 
        Json::parseFromStream(rbuilder, in, &extractionConfigJSON, &errs))) {
        cerr << "Error reading config file!"<< endl;
        cerr << errs.c_str() << endl;
    }
    return extractionConfigJSON;
}
istream& operator>>(istream &in, ExtractionConfig &config)
{
    Json::Value configJson = config.readConfigFile(in);

    VariabilityOptions *vOpts = new VariabilityOptions;
    LanguageFeatureOptions *lFeats = new LanguageFeatureOptions;
    ROSFeatureOptions *ROSFeats = new ROSFeatureOptions;
    GuidedExtractionOptions *geOpts = new GuidedExtractionOptions;
    if (!configJson.empty()){

        for (auto configOption : configJson.getMemberNames()){
            RexArgs::ConfigType type = RexArgs::stringToConfig(configOption);

            switch (type)
            {
            case RexArgs::VARIABILITY:
                {
                    vOpts->readJson(configJson);
                    break;
                }

            case RexArgs::LANG_FEATURES:
                {
                    lFeats->readJson(configJson);
                    break;    
                }         
            case RexArgs::ROS_FEATURES:
                {
                    ROSFeats->readJson(configJson);        
                    break;    
                }
                
            case RexArgs::GUIDED_EXTRACTION:
                {
                    geOpts->readJson(configJson);
                    break;
                }
            default:
                break;
            }
        }
    }
    else {
        cerr << "Empty config file! Using default options." << endl;
    }

    config.addOption(RexArgs::configToString(RexArgs::VARIABILITY), vOpts);
    config.addOption(RexArgs::configToString(RexArgs::LANG_FEATURES), lFeats);
    config.addOption(RexArgs::configToString(RexArgs::ROS_FEATURES), ROSFeats);
    config.addOption(RexArgs::configToString(RexArgs::GUIDED_EXTRACTION), geOpts);
    
    return in;
}
bool VariabilityOptions::isEqual(const ConfigOption &other) const
{
    const VariabilityOptions otherCpy = *dynamic_cast<const VariabilityOptions*>(&other);
    return globalsOnly == otherCpy.globalsOnly
        && enums == otherCpy.enums
        && ints == otherCpy.ints
        && bools == otherCpy.bools
        && chars == otherCpy.chars
        && nameMatch == otherCpy.nameMatch
        && variability_On == otherCpy.variability_On;
}
string VariabilityOptions::writeData() const
{
    string result;
    result = globalsOnly ? result+"1|":result+"0|";
    result = enums ? result+"1|":result+"0|";
    result = ints ? result+"1|":result+"0|";
    result = bools ? result+"1|":result+"0|";
    result = chars ? result+"1|":result+"0|";
    result = result + nameMatch;
    return result;
}
istream& VariabilityOptions::readFile(std::istream &in)
{
    Json::Value extractionConfig;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    if (in.good() && 
        Json::parseFromStream(rbuilder, in, &extractionConfig, &errs)) {
        globalsOnly = extractionConfig["variability"]["globalsOnly"].asBool();
        enums = extractionConfig["variability"]["enums"].asBool();
        ints = extractionConfig["variability"]["ints"].asBool();
        bools = extractionConfig["variability"]["bools"].asBool();
        chars = extractionConfig["variability"]["chars"].asBool();
        nameMatch = extractionConfig["variability"]["nameMatch"].asString();
        variability_On = extractionConfig["variability"]["variability_On"].asBool();
    }
    else
    {   
        cerr << "Error reading config file!"<< endl;
        cerr << errs << endl;
    }
    return in;
}

bool VariabilityOptions::readJson(Json::Value extractionConfig){
    if (!extractionConfig.empty()) {
        globalsOnly = extractionConfig["variability"]["globalsOnly"].asBool();
        enums = extractionConfig["variability"]["enums"].asBool();
        ints = extractionConfig["variability"]["ints"].asBool();
        bools = extractionConfig["variability"]["bools"].asBool();
        chars = extractionConfig["variability"]["chars"].asBool();
        nameMatch = extractionConfig["variability"]["nameMatch"].asString();
        variability_On = extractionConfig["variability"]["variability_On"].asBool();
        return true;
    }
    else{
        return false;
    }
}

bool LanguageFeatureOptions::isEqual(const ConfigOption &other) const
{
    const LanguageFeatureOptions otherCpy = *dynamic_cast<const LanguageFeatureOptions*>(&other);
    return cClass == otherCpy.cClass
        && cStruct == otherCpy.cStruct
        && cUnion == otherCpy.cUnion
        && cEnum == otherCpy.cEnum
        && cFunction == otherCpy.cFunction
        && cVariable == otherCpy.cVariable;
}

string LanguageFeatureOptions::writeData() const
{
    string result;
    result = cClass ? result+"1|":result+"0|";
    result = cStruct ? result+"1|":result+"0|";
    result = cUnion ? result+"1|":result+"0|";
    result = cEnum ? result+"1|":result+"0|";
    result = cFunction ? result+"1|":result+"0|";
    result = cVariable ? result+"1|":result+"0|";
    return result;
}

istream& LanguageFeatureOptions::readFile(std::istream &in)
{
    Json::Value extractionConfig;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    if (in.good() && 
        Json::parseFromStream(rbuilder, in, &extractionConfig, &errs)) {
        cClass = extractionConfig["langFeatures"]["cClass"].asBool();
        cStruct = extractionConfig["langFeatures"]["cStruct"].asBool();
        cUnion = extractionConfig["langFeatures"]["cUnion"].asBool();
        cEnum = extractionConfig["langFeatures"]["cEnum"].asBool();
        cFunction = extractionConfig["langFeatures"]["cFunction"].asBool();
        cVariable = extractionConfig["langFeatures"]["cVariable"].asBool();
    }
    else
    {   
        cerr << "Error reading config file!"<< endl;
        cerr << errs << endl;
    }
    return in;
}

bool LanguageFeatureOptions::readJson(Json::Value extractionConfig){
    if (!extractionConfig.empty()) {
        cClass = extractionConfig["langFeatures"]["cClass"].asBool();
        cStruct = extractionConfig["langFeatures"]["cStruct"].asBool();
        cUnion = extractionConfig["langFeatures"]["cUnion"].asBool();
        cEnum = extractionConfig["langFeatures"]["cEnum"].asBool();
        cFunction = extractionConfig["langFeatures"]["cFunction"].asBool();
        cVariable = extractionConfig["langFeatures"]["cVariable"].asBool();
        return true;
    }
    else{
        return false;
    }
}

bool ROSFeatureOptions::isEqual(const ConfigOption &other) const
{
    const ROSFeatureOptions otherCpy = *dynamic_cast<const ROSFeatureOptions*>(&other);
    return appendFeatureName == otherCpy.appendFeatureName;
}

string ROSFeatureOptions::writeData() const
{
    string result;
    result = appendFeatureName ? result+"1|":result+"0|";
    return result;
}

istream& ROSFeatureOptions::readFile(std::istream &in)
{
    Json::Value extractionConfig;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    if (in.good() && 
        Json::parseFromStream(rbuilder, in, &extractionConfig, &errs)) {
        appendFeatureName = extractionConfig["ROSFeatures"]["appendFeatureName"].asBool();
    }
    else
    {   
        cerr << "Error reading config file!"<< endl;
    }
    return in;
}

bool ROSFeatureOptions::readJson(Json::Value extractionConfig){
    if (!extractionConfig.empty()) {
        appendFeatureName = extractionConfig["ROSFeatures"]["appendFeatureName"].asBool();
        return true;
    }
    else{
        return false;
    }
}

bool GuidedExtractionOptions::isEqual(const ConfigOption &other) const
{
    const GuidedExtractionOptions otherCpy = *dynamic_cast<const GuidedExtractionOptions*>(&other);
    return callbackFuncRegex == otherCpy.callbackFuncRegex;
}

string GuidedExtractionOptions::writeData() const
{
    return callbackFuncRegex;
}

istream& GuidedExtractionOptions::readFile(std::istream &in)
{
    Json::Value extractionConfig;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    if (in.good() && 
        Json::parseFromStream(rbuilder, in, &extractionConfig, &errs)) {
        callbackFuncRegex = extractionConfig["guidedExtraction"]["callbackFuncRegex"].asString();
    }
    else
    {   
        cerr << "Error reading config file!"<< endl;
        cerr << errs << endl;
    }
    return in;
}

bool GuidedExtractionOptions::readJson(Json::Value extractionConfig){
    if (!extractionConfig.empty()) {
        callbackFuncRegex = extractionConfig["guidedExtraction"]["callbackFuncRegex"].asString();
        return true;
    }
    else{
        return false;
    }
}