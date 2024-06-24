#pragma once
#include <unordered_map>
#include <typeinfo>
#include <regex>
#include <string>
#include "../JSON/json.h"
// Encapsualtes a base Configuration Option, which should be 
// extended to options we support (e.g. Variability-aware)
struct ConfigOption 
{
    virtual ~ConfigOption();
    bool operator==(const ConfigOption & other) const{
        if(typeid(*this) == typeid(other))
        {
          return isEqual(other);
        }
        return false;
    }
    bool operator!=(const ConfigOption &other) const {
        return !(*this == other);
    }
    virtual bool isEqual(const ConfigOption &other) const = 0;
    virtual std::string writeData() const = 0;
    virtual std::istream& readFile(std::istream &in) = 0;
    virtual bool readJson(Json::Value extractionConfig) = 0;
    
    friend std::istream& operator>>(std::istream &in, ConfigOption &config){
        return config.readFile(in);
    }
    friend std::ostream& operator<<(std::ostream &out, const ConfigOption &config){
        out << config.writeData();
        return out;
    }

    
};
// Variability aware Configuration option
struct VariabilityOptions : public ConfigOption {
    bool globalsOnly = false;
    bool nonglobalsOnly = false;
    bool enums = false;
    bool ints = false;
    bool bools = false;
    bool chars = false;
    bool variability_On = false;
    bool extractAll = false;
    std::string nameMatch = "";
    std::regex cfgVarRegex;
    
    // Do we extract all variables as config variables
    bool allVariables() const {
        return (!globalsOnly && !nonglobalsOnly && 
                !enums && !ints && !bools && !chars && nameMatch=="");
    }
    
    virtual bool isEqual(const ConfigOption &other) const override;
    virtual std::string writeData() const override;
    virtual std::istream& readFile(std::istream &in) override;
    virtual bool readJson(Json::Value extractionConfig) override;
    
    // Do we care about the type of variables? If this is false
    // then no check need type of variables.
    bool typeSpecific() const {
        return (enums || ints || bools || chars);
    }

    bool isOn() const {
        return (extractAll || globalsOnly || nonglobalsOnly ||
                ints || bools || chars || enums || variability_On || nameMatch != "");
    }
    
};

// Language features Configuration options
struct LanguageFeatureOptions : public ConfigOption {
    bool cClass = true;
    bool cStruct = true;
    bool cUnion = true;
    bool cEnum = true;
    bool cFunction = true;
    bool cVariable = true;
    
    // bool getClass(){
    //     return cClass;
    // }

    bool AllFeaturesEnabled() const {
        return ( cClass && cStruct && cUnion && 
                cEnum && cFunction && cVariable);
    }

    virtual bool isEqual(const ConfigOption &other) const override;
    virtual std::string writeData() const override;
    virtual std::istream& readFile(std::istream &in) override;
    virtual bool readJson(Json::Value extractionConfig) override;
};

// User-provided criteria for attribute extraction
struct GuidedExtractionOptions : public ConfigOption {
    // Functions with matching names are determined to be callbacks.
    std::string callbackFuncRegex;

    virtual bool isEqual(const ConfigOption &other) const override;
    virtual std::string writeData() const override;
    virtual std::istream& readFile(std::istream &in) override;
    virtual bool readJson(Json::Value extractionConfig) override;
};

// ROS features options
struct ROSFeatureOptions : public ConfigOption {
    bool appendFeatureName = true;

    virtual bool isEqual(const ConfigOption &other) const override;
    virtual std::string writeData() const;
    virtual std::istream& readFile(std::istream &in);
    virtual bool readJson(Json::Value extractionConfig);
};

// This enapsulates the configuration options that we support
// for extraction, e.g. Variability Options.

// One important thing to note is that an ExtractionConfig owns
// the Configuration objects it stores via its pointers
class ExtractionConfig 
{
    
    public:
        ExtractionConfig();
        virtual ~ExtractionConfig();
        
        typedef std::unordered_map<std::string, ConfigOption*>::iterator iterator;
        typedef std::unordered_map<std::string, ConfigOption*>::const_iterator const_iterator;
        
        Json::Value readConfigFile(std::istream &in);
        void addOption(std::string option, ConfigOption *value);
        iterator find(std::string key);
        const_iterator find(std::string key) const;
        bool hasKey(std::string key) const;
        ConfigOption& get(std::string key);
        const ConfigOption& get(std::string key) const;
        unsigned int size() const;
        bool operator==(const ExtractionConfig &other) const;
    
        iterator begin();
        iterator end();
        
        const_iterator begin() const;
        const_iterator end() const;
        
        friend std::istream& operator>>(std::istream &in, ExtractionConfig &config);
        
    
    private:
        // ExtractionConfig owns the ConfigOption ptrs
        std::unordered_map<std::string, ConfigOption*> options;
    
};
