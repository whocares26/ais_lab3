#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "db/Term.hpp"

namespace db {
    struct LinguisticVariable {
        std::string m_title;        // подпись для протокола и графиков
        std::string m_unit;         // единицы измерения
        Universe m_universe;
        std::vector<Term> m_terms;  // массив, а не объект json: важен порядок термов
    };

    inline const Term* findTerm(const LinguisticVariable& var, const std::string& name) {
        for (const auto& t : var.m_terms)
            if (t.m_name == name)
                return &t;
        return nullptr;
    }

    inline void from_json(const nlohmann::json& j, LinguisticVariable& var) {
        var.m_title = j.value("title", "");
        var.m_unit = j.value("unit", "");
        j.at("universe").get_to(var.m_universe);
        j.at("terms").get_to(var.m_terms);
    }
}
