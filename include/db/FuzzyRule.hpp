#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace db {
    // нечёткое высказывание «переменная есть терм»
    struct Statement {
        std::string m_var;
        std::string m_term;
    };

    struct FuzzyRule {
        std::vector<Statement> m_condition;
        Statement m_result;
        int m_id;
    };

    inline std::string toString(const Statement& s) {
        return s.m_var + " есть " + s.m_term;
    }

    inline std::string toString(const FuzzyRule& rule) {
        std::string res = "ЕСЛИ ";
        for (size_t i = 0; i < rule.m_condition.size(); i++) {
            if (i > 0)
                res += " И ";
            res += toString(rule.m_condition[i]);
        }
        return res + " ТО " + toString(rule.m_result);
    }

    inline void from_json(const nlohmann::json& j, Statement& s) {
        j.at("var").get_to(s.m_var);
        j.at("term").get_to(s.m_term);
    }

    inline void from_json(const nlohmann::json& j, FuzzyRule& rule) {
        j.at("id").get_to(rule.m_id);
        j.at("if").get_to(rule.m_condition);
        j.at("then").get_to(rule.m_result);
    }
}
