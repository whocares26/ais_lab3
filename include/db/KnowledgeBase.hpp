#pragma once
#include <string>
#include <vector>
#include <map>
#include "db/LinguisticVariable.hpp"
#include "db/FuzzyRule.hpp"
#include "db/Scenario.hpp"

namespace db {
    inline const std::string db_path = "../data/funnel.json";
    inline const std::string lecture_path = "../data/lecture_example.json";
    inline const std::string plots_path = "../plots";

    class KnowledgeBase {
    public:
        bool load(const std::string& path);
        const std::map<std::string, LinguisticVariable>& getVariables() const;
        const LinguisticVariable& getVariable(const std::string& name) const;
        const std::string& getSingleInput() const;
        const std::vector<std::string>& getMultiInputs() const;
        const std::string& getOutput() const;
        const std::vector<FuzzyRule>& getSingleRules() const;
        const std::vector<FuzzyRule>& getMultiRules() const;
        const std::vector<Scenario>& getScenarios() const;
        const Scenario* findScenario(const std::string& system) const;   // первый сценарий системы
        const std::string& getPath() const;
    private:
        bool check() const;   // все ссылки на переменные и термы существуют
        std::map<std::string, LinguisticVariable> m_variables;
        std::string m_single_input;
        std::vector<std::string> m_multi_inputs;
        std::string m_output;
        std::vector<FuzzyRule> m_single_rules;
        std::vector<FuzzyRule> m_multi_rules;
        std::vector<Scenario> m_scenarios;
        std::string m_path;
    };
}
