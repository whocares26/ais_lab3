#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include "db/KnowledgeBase.hpp"
#include "engine/SelfTest.hpp"
#include "engine/Inference.hpp"
#include "engine/FuzzyMath.hpp"
#include "engine/Colors.hpp"

namespace engine {

        static const double EPS = 0.01;   // допуск: в лекции числа даны с двумя знаками

        struct Check {
            std::string m_case;
            std::string m_expected;
            std::string m_got;
            bool m_ok;
        };

        static std::string vecStr(const std::vector<double>& v) {
            std::string res = "(";
            for (size_t i = 0; i < v.size(); i++)
                res += (i ? "; " : "") + fmtTrim(v[i], 3);
            return res + ")";
        }

        static bool close(const std::vector<double>& a, const std::vector<double>& b) {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); i++)
                if (std::fabs(a[i] - b[i]) > EPS)
                    return false;
            return true;
        }

        static Check checkSet(const std::string& name, const std::vector<double>& expected, const std::vector<double>& got) {
            return {name, vecStr(expected), vecStr(got), close(expected, got)};
        }

        // expected < 0 — ожидается пустое B′
        static Check checkY(const std::string& name, double expected, const InferenceResult& r) {
            std::string got = r.m_defined ? fmt(r.m_crisp_output, 3) : "B′ пустое";
            if (expected < 0)
                return {name, "B′ пустое", got, !r.m_defined};
            return {name, fmt(expected, 2), got, r.m_defined && std::fabs(r.m_crisp_output - expected) <= EPS};
        }

        static const db::Scenario* byName(const db::KnowledgeBase& kb, const std::string& name) {
            for (const auto& s : kb.getScenarios())
                if (s.m_name == name)
                    return &s;
            return nullptr;
        }

        bool runSelfTest(const std::string& lecturePath, const db::KnowledgeBase& funnel) {
            db::KnowledgeBase lec;
            if (!lec.load(lecturePath)) {
                std::cout << RED << "Не удалось загрузить " << lecturePath << RESET << "\n";
                return false;
            }
            const db::Scenario* s1 = byName(lec, "давление_среднее");
            const db::Scenario* s2 = byName(lec, "давление_среднее_объём_достаточно_маленький");
            const db::Scenario* s3 = byName(lec, "давление_860_объём_520");
            if (s1 == nullptr || s2 == nullptr || s3 == nullptr) {
                std::cout << RED << "В " << lecturePath << " нет нужных сценариев" << RESET << "\n";
                return false;
            }

            std::vector<Check> checks;
            Inference li(lec);
            FuzzyInput a = li.makeInputs(*s1)[0];

            // одно правило R1, Мамдани (лекция 4, слайд 8)
            {
                const auto& x = lec.getVariable(lec.getSingleInput());
                const auto& y = lec.getVariable(lec.getOutput());
                FuzzySet A1 = db::termValues(*db::findTerm(x, "большое"), x.m_universe);
                FuzzySet B1 = db::termValues(*db::findTerm(y, "средняя"), y.m_universe);
                checks.push_back(checkSet("Лекция: только R1, Мамдани, B′", {0.5, 0.8, 0.5},
                                          compose(a.m_set, relation(A1, B1, Implication::Mamdani))));
            }

            for (int alg = 1; alg <= 3; alg++) {
                InferenceResult r = li.runSingle(a, Implication::Mamdani, Aggregation::Max, alg);
                std::string name = "Лекция: Мамдани, max, алг. " + std::to_string(alg);
                checks.push_back(checkSet(name + ", B′", {0.8, 0.8, 0.5}, r.m_output));
                checks.push_back(checkY(name + ", y*", 342.857, r));
            }
            for (int alg = 1; alg <= 3; alg++) {
                InferenceResult r = li.runSingle(a, Implication::Larsen, Aggregation::Max, alg);
                std::vector<double> expected = alg == 3 ? std::vector<double>{0.8, 0.8, 0.4} : std::vector<double>{0.8, 0.8, 0.5};
                checks.push_back(checkSet("Лекция: Ларсен, max, алг. " + std::to_string(alg) + ", B′", expected, r.m_output));
            }
            checks.push_back(checkSet("Лекция: Мамдани, вер. сумма, алг. 1, B′", {0.9, 0.88, 0.55},
                                      li.runSingle(a, Implication::Mamdani, Aggregation::ProbSum, 1).m_output));
            checks.push_back(checkSet("Лекция: Мамдани, вер. сумма, алг. 2, B′", {0.8, 0.88, 0.55},
                                      li.runSingle(a, Implication::Mamdani, Aggregation::ProbSum, 2).m_output));

            // два входа (слайды 20–23) и чёткие входы (слайды 28–29)
            {
                InferenceResult r = li.runMulti(li.makeInputs(*s2), Implication::Mamdani, Aggregation::Max);
                checks.push_back(checkSet("Лекция, 2 входа: α", {0.3, 0.3, 0.8}, r.m_alpha));
                checks.push_back(checkSet("Лекция, 2 входа: Мамдани, B′", {0.5, 0.8, 0.5}, r.m_output));
                checks.push_back(checkY("Лекция, 2 входа: Мамдани, y*", 350, r));
                InferenceResult c = li.runMulti(li.makeInputs(*s3), Implication::Mamdani, Aggregation::Max);
                checks.push_back(checkSet("Лекция, x1 = 860, x2 = 520: α", {0.3, 0.3, 0.8}, c.m_alpha));
            }

            // воронка, п. 2: «примерно 45% от цели»
            Inference fi(funnel);
            const db::Scenario* f1 = funnel.findScenario("single");
            const db::Scenario* f2 = funnel.findScenario("multi");
            if (f1 != nullptr) {
                FuzzyInput in = fi.makeInputs(*f1)[0];
                checks.push_back(checkSet("Воронка п. 2: α", {0, 0.25, 0.75, 0.5, 0, 0, 0},
                                          fi.runSingle(in, Implication::Mamdani, Aggregation::Max, 3).m_alpha));
                struct Row { Implication imp; double y[3]; };
                std::vector<Row> rows = {
                    {Implication::Mamdani,      {3.81, 3.81, 3.81}},
                    {Implication::Larsen,       {3.92, 3.92, 3.83}},
                    {Implication::Lukasiewicz,  {3.10, 3.05, 3.21}},
                    {Implication::KleeneDienes, {3.04, 3.04, 3.19}},
                    {Implication::Godel,        {63.0 / 19, 3.700, -1}},   // точный расчёт дробями: 63/19 и 37/10
                };
                for (const auto& row : rows)
                    for (int alg = 1; alg <= 3; alg++) {
                        Aggregation agg = defaultAggregation(row.imp);
                        checks.push_back(checkY("Воронка п. 2: " + toString(row.imp) + ", " + toString(agg) + ", алг. "
                                                + std::to_string(alg) + ", y*", row.y[alg - 1],
                                                fi.runSingle(in, row.imp, agg, alg)));
                    }
            }

            // воронка, п. 3: (50, 60, 30)
            if (f2 != nullptr) {
                std::vector<FuzzyInput> in = fi.makeInputs(*f2);
                checks.push_back(checkSet("Воронка п. 3: α", {0.5, 0, 0, 0, 0.33, 0, 0, 0, 0},
                                          fi.runMulti(in, Implication::Mamdani, Aggregation::Max).m_alpha));
                checks.push_back(checkY("Воронка п. 3: Мамдани, max, y*", 4.375,
                                        fi.runMulti(in, Implication::Mamdani, Aggregation::Max)));
                checks.push_back(checkY("Воронка п. 3: Ларсен, max, y*", 4.50,
                                        fi.runMulti(in, Implication::Larsen, Aggregation::Max)));
                checks.push_back(checkY("Воронка п. 3: Лукасевич, min, y*", 3.13,
                                        fi.runMulti(in, Implication::Lukasiewicz, Aggregation::Min)));
            }

            std::cout << CYAN << BOLD << "\n=== Самопроверка (допуск " << fmt(EPS, 2) << ") ===" << RESET << "\n";
            std::cout << BOLD << pad("Проверка", 43) << pad("Ожидалось", 25) << pad("Получено", 25) << "Итог" << RESET << "\n";
            int passed = 0;
            for (const auto& c : checks) {
                std::cout << pad(c.m_case, 42) << " " << pad(c.m_expected, 24) << " " << pad(c.m_got, 24) << " "
                          << (c.m_ok ? GREEN "ОК" : RED "ОШИБКА") << RESET << "\n";
                if (c.m_ok)
                    passed++;
            }
            bool all = passed == (int)checks.size();
            std::cout << (all ? GREEN : RED) << BOLD << "Пройдено " << passed << " из " << checks.size() << RESET << "\n";
            return all;
        }
    }
