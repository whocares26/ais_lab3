#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace db {
    // вход одной переменной
    struct InputSpec {
        std::string m_var;
        std::string m_kind;             // crisp | term | tri | trap | points
        double m_value;                 // crisp: чёткое число
        std::string m_term;             // term: имя терма
        std::string m_modifier;         // term: "", "очень" или "более_менее"
        std::vector<double> m_params;   // tri, trap: параметры; points: значения в узлах
    };

    // именованный набор входов для отчёта
    struct Scenario {
        std::string m_name;
        std::string m_title;
        std::string m_system;           // single | multi
        std::vector<InputSpec> m_inputs;
    };

    inline void from_json(const nlohmann::json& j, InputSpec& in) {
        j.at("var").get_to(in.m_var);
        j.at("kind").get_to(in.m_kind);
        in.m_value = j.value("value", 0.0);
        in.m_term = j.value("term", "");
        in.m_modifier = j.value("modifier", "");
        in.m_params = j.value("params", std::vector<double>());
    }

    inline void from_json(const nlohmann::json& j, Scenario& s) {
        j.at("name").get_to(s.m_name);
        s.m_title = j.value("title", s.m_name);
        j.at("system").get_to(s.m_system);
        j.at("inputs").get_to(s.m_inputs);
    }
}
