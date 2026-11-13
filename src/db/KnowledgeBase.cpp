#include "db/KnowledgeBase.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <exception>

namespace db {
    bool KnowledgeBase::load(const std::string& path) {
        using json = nlohmann::json;
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        try {
            json data = json::parse(file);
            file.close();
            m_variables = data["variables"].get<std::map<std::string, LinguisticVariable>>();
            m_single_input = data["single_input"].get<std::string>();
            m_multi_inputs = data["multi_inputs"].get<std::vector<std::string>>();
            m_output = data["output"].get<std::string>();
            m_single_rules = data["single_input_rules"].get<std::vector<FuzzyRule>>();
            m_multi_rules = data["multi_input_rules"].get<std::vector<FuzzyRule>>();
            m_scenarios = data.value("scenarios", std::vector<Scenario>());
        } catch(const std::exception& e) {
            std::cout<<e.what()<<std::endl;
            return false;
        }
        m_path = path;
        return check();
    }

    bool KnowledgeBase::check() const {
        // переменная и терм существуют
        auto termOk = [this](const std::string& var, const std::string& term) {
            auto it = m_variables.find(var);
            if (it == m_variables.end()) {
                std::cout << "Ошибка в базе знаний: нет переменной " << var << "\n";
                return false;
            }
            if (!term.empty() && findTerm(it->second, term) == nullptr) {
                std::cout << "Ошибка в базе знаний: у переменной " << var << " нет терма " << term << "\n";
                return false;
            }
            return true;
        };

        // число параметров термов
        for (const auto& pair : m_variables) {
            size_t n = grid(pair.second.m_universe).size();
            for (const auto& t : pair.second.m_terms) {
                bool ok = (t.m_type == "tri" && t.m_params.size() == 3)
                       || (t.m_type == "trap" && t.m_params.size() == 4)
                       || (t.m_type == "points" && t.m_params.size() == n);
                if (!ok) {
                    std::cout << "Ошибка в базе знаний: неверный терм " << pair.first << "." << t.m_name << "\n";
                    return false;
                }
            }
        }

        if (!termOk(m_single_input, "") || !termOk(m_output, ""))
            return false;
        for (const auto& name : m_multi_inputs)
            if (!termOk(name, ""))
                return false;

        for (const auto& rule : m_single_rules) {
            if (rule.m_condition.size() != 1 || rule.m_condition[0].m_var != m_single_input
                || rule.m_result.m_var != m_output) {
                std::cout << "Ошибка в базе знаний: правило " << rule.m_id << " не относится к системе с одним входом\n";
                return false;
            }
            if (!termOk(rule.m_condition[0].m_var, rule.m_condition[0].m_term)
                || !termOk(rule.m_result.m_var, rule.m_result.m_term))
                return false;
        }

        for (const auto& rule : m_multi_rules) {
            for (const auto& cond : rule.m_condition) {
                if (std::find(m_multi_inputs.begin(), m_multi_inputs.end(), cond.m_var) == m_multi_inputs.end()) {
                    std::cout << "Ошибка в базе знаний: в правиле " << rule.m_id << " переменная "
                              << cond.m_var << " не входит в multi_inputs\n";
                    return false;
                }
                if (!termOk(cond.m_var, cond.m_term))
                    return false;
            }
            if (rule.m_result.m_var != m_output || !termOk(rule.m_result.m_var, rule.m_result.m_term))
                return false;
        }

        for (const auto& s : m_scenarios) {
            for (const auto& in : s.m_inputs) {
                if (!termOk(in.m_var, in.m_kind == "term" ? in.m_term : ""))
                    return false;
                size_t n = grid(m_variables.at(in.m_var).m_universe).size();
                bool ok = in.m_kind == "crisp" || in.m_kind == "term"
                       || (in.m_kind == "tri" && in.m_params.size() == 3)
                       || (in.m_kind == "trap" && in.m_params.size() == 4)
                       || (in.m_kind == "points" && in.m_params.size() == n);
                if (!ok) {
                    std::cout << "Ошибка в базе знаний: неверный вход сценария " << s.m_name << "\n";
                    return false;
                }
            }
        }
        return true;
    }

    const std::map<std::string, LinguisticVariable>& KnowledgeBase::getVariables() const {
        return m_variables;
    }
    const LinguisticVariable& KnowledgeBase::getVariable(const std::string& name) const {
        return m_variables.at(name);
    }
    const std::string& KnowledgeBase::getSingleInput() const {
        return m_single_input;
    }
    const std::vector<std::string>& KnowledgeBase::getMultiInputs() const {
        return m_multi_inputs;
    }
    const std::string& KnowledgeBase::getOutput() const {
        return m_output;
    }
    const std::vector<FuzzyRule>& KnowledgeBase::getSingleRules() const {
        return m_single_rules;
    }
    const std::vector<FuzzyRule>& KnowledgeBase::getMultiRules() const {
        return m_multi_rules;
    }
    const std::vector<Scenario>& KnowledgeBase::getScenarios() const {
        return m_scenarios;
    }
    const Scenario* KnowledgeBase::findScenario(const std::string& system) const {
        for (const auto& s : m_scenarios)
            if (s.m_system == system)
                return &s;
        return nullptr;
    }
    const std::string& KnowledgeBase::getPath() const {
        return m_path;
    }
}
