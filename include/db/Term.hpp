#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <nlohmann/json.hpp>

namespace db {
    // универсум: равномерная сетка min..max с шагом step или явный список точек
    struct Universe {
        double m_min;
        double m_max;
        double m_step;
        std::vector<double> m_points;   // если не пусто — сетка неравномерная (пример из лекции)
    };

    inline std::vector<double> grid(const Universe& u) {
        if (!u.m_points.empty())
            return u.m_points;
        std::vector<double> res;
        int n = (int)std::round((u.m_max - u.m_min) / u.m_step);
        for (int i = 0; i <= n; i++)
            res.push_back(u.m_min + i * u.m_step);
        return res;
    }

    inline void from_json(const nlohmann::json& j, Universe& u) {
        u.m_points.clear();
        if (j.contains("points")) {
            j.at("points").get_to(u.m_points);
            u.m_min = u.m_points.front();
            u.m_max = u.m_points.back();
            u.m_step = 0;
        } else {
            j.at("min").get_to(u.m_min);
            j.at("max").get_to(u.m_max);
            j.at("step").get_to(u.m_step);
        }
    }

    // терм лингвистической переменной
    struct Term {
        std::string m_name;
        std::string m_type;             // tri | trap | points
        std::vector<double> m_params;   // tri: a, b, c; trap: a, b, c, d; points: значения в узлах сетки
    };

    // трапеция с плато [b; c]; треугольник — частный случай b = c
    inline double trapezoid(double a, double b, double c, double d, double x) {
        if (x >= b && x <= c)
            return 1.0;
        if (x < b)
            return x <= a ? 0.0 : (x - a) / (b - a);
        return x >= d ? 0.0 : (d - x) / (d - c);
    }

    // μ терма в произвольной точке x (не только в узле сетки)
    inline double membership(const Term& t, const Universe& u, double x) {
        const auto& p = t.m_params;
        if (t.m_type == "tri")
            return trapezoid(p[0], p[1], p[1], p[2], x);
        if (t.m_type == "trap")
            return trapezoid(p[0], p[1], p[2], p[3], x);

        // points — линейная интерполяция между узлами
        std::vector<double> xs = grid(u);
        if (x <= xs.front())
            return x == xs.front() ? p.front() : 0.0;
        if (x >= xs.back())
            return x == xs.back() ? p.back() : 0.0;
        for (size_t i = 1; i < xs.size(); i++) {
            if (x <= xs[i]) {
                double k = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
                return p[i - 1] + k * (p[i] - p[i - 1]);
            }
        }
        return 0.0;
    }

    // значения терма в узлах сетки
    inline std::vector<double> termValues(const Term& t, const Universe& u) {
        if (t.m_type == "points")
            return t.m_params;
        std::vector<double> res;
        for (double x : grid(u))
            res.push_back(membership(t, u, x));
        return res;
    }

    inline std::string toString(const Term& t) {
        std::string res = t.m_type + " [";
        for (size_t i = 0; i < t.m_params.size(); i++) {
            if (i > 0)
                res += "; ";
            std::string num = std::to_string(t.m_params[i]);
            num.erase(num.find_last_not_of('0') + 1);   // 20.000000 -> 20.
            if (num.back() == '.')
                num.pop_back();
            for (auto& c : num)
                if (c == '.') c = ',';
            res += num;
        }
        return res + "]";
    }

    inline void from_json(const nlohmann::json& j, Term& t) {
        j.at("name").get_to(t.m_name);
        j.at("type").get_to(t.m_type);
        j.at("params").get_to(t.m_params);
    }
}
