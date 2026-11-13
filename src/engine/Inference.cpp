#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "db/KnowledgeBase.hpp"
#include "engine/Inference.hpp"
#include "engine/Colors.hpp"

namespace engine {

        // индекс ближайшего узла сетки
        static size_t nearestIndex(const std::vector<double>& xs, double x) {
            size_t best = 0;
            for (size_t i = 1; i < xs.size(); i++)
                if (std::fabs(xs[i] - x) < std::fabs(xs[best] - x))
                    best = i;
            return best;
        }

        std::string algorithmName(int algorithm) {
            switch (algorithm) {
                case 1: return "алгоритм 1 (первоначальное вычисление выходов правил)";
                case 2: return "алгоритм 2 (первоначальная агрегация правил)";
                case 3: return "алгоритм 3 (уровни истинности предпосылок)";
            }
            return "";
        }

        FuzzyInput Inference::makeInput(const db::InputSpec& spec) const {
            const auto& var = m_kb.getVariable(spec.m_var);
            std::vector<double> xs = db::grid(var.m_universe);
            FuzzyInput in;
            in.m_var = spec.m_var;

            if (spec.m_kind == "crisp") {
                in.m_crisp = true;
                in.m_value = spec.m_value;
                in.m_set.assign(xs.size(), 0.0);
                in.m_set[nearestIndex(xs, spec.m_value)] = 1.0;
                in.m_label = fmtTrim(spec.m_value, 2);
            } else if (spec.m_kind == "term") {
                in.m_set = applyModifier(db::termValues(*db::findTerm(var, spec.m_term), var.m_universe), spec.m_modifier);
                in.m_label = (spec.m_modifier.empty() ? "" : display(spec.m_modifier) + " ") + spec.m_term;
            } else if (spec.m_kind == "points") {
                in.m_set = spec.m_params;
                in.m_label = "по точкам";
            } else {
                db::Term t{"", spec.m_kind, spec.m_params};   // tri или trap
                in.m_set = db::termValues(t, var.m_universe);
                in.m_label = db::toString(t);
            }
            return in;
        }

        std::vector<FuzzyInput> Inference::makeInputs(const db::Scenario& s) const {
            std::vector<FuzzyInput> res;
            for (const auto& spec : s.m_inputs)
                res.push_back(makeInput(spec));
            if (res.size() == 1 && s.m_inputs[0].m_kind != "crisp")
                res[0].m_label = display(s.m_title);   // нечёткий вход называем по сценарию
            return res;
        }

        InferenceResult Inference::runSingle(const FuzzyInput& in, Implication imp, Aggregation agg, int algorithm) const {
            InferenceResult r;
            r.m_implication = imp;
            r.m_aggregation = agg;
            r.m_algorithm = algorithm;

            const auto& inVar = m_kb.getVariable(m_kb.getSingleInput());
            const auto& outVar = m_kb.getVariable(m_kb.getOutput());
            std::vector<double> xs = db::grid(inVar.m_universe);
            std::vector<double> ys = db::grid(outVar.m_universe);
            const auto& rules = m_kb.getSingleRules();

            // A′: для чёткого входа — синглтон в ближайшем узле
            FuzzySet A = in.m_set;
            if (in.m_crisp) {
                size_t k = nearestIndex(xs, in.m_value);
                A.assign(xs.size(), 0.0);
                A[k] = 1.0;
                if (algorithm != 3 && std::fabs(xs[k] - in.m_value) > 1e-9) {
                    r.m_snapped = true;
                    r.m_snapped_x = xs[k];
                }
            }

            std::vector<FuzzySet> As, Bs;
            for (const auto& rule : rules) {
                const db::Term* a = db::findTerm(inVar, rule.m_condition[0].m_term);
                const db::Term* b = db::findTerm(outVar, rule.m_result.m_term);
                As.push_back(db::termValues(*a, inVar.m_universe));
                Bs.push_back(db::termValues(*b, outVar.m_universe));

                // уровень истинности: для чёткого входа — по параметрам терма
                double alpha = in.m_crisp ? db::membership(*a, inVar.m_universe, in.m_value)
                                          : height(A, As.back());
                r.m_alpha.push_back(alpha);
                r.m_input_mu.push_back({alpha});
            }

            if (algorithm == 1) {
                for (size_t i = 0; i < rules.size(); i++) {
                    r.m_relations.push_back(relation(As[i], Bs[i], imp));
                    r.m_rule_outputs.push_back(compose(A, r.m_relations.back()));
                }
                r.m_output = aggregateSets(r.m_rule_outputs, agg, ys.size());
            } else if (algorithm == 2) {
                for (size_t i = 0; i < rules.size(); i++)
                    r.m_relations.push_back(relation(As[i], Bs[i], imp));
                r.m_aggregated = aggregateMatrices(r.m_relations, agg, xs.size(), ys.size());
                r.m_output = compose(A, r.m_aggregated);
            } else {
                for (size_t i = 0; i < rules.size(); i++) {
                    FuzzySet out(ys.size());
                    for (size_t y = 0; y < ys.size(); y++)
                        out[y] = implicate(imp, r.m_alpha[i], Bs[i][y]);
                    r.m_rule_outputs.push_back(out);
                }
                r.m_output = aggregateSets(r.m_rule_outputs, agg, ys.size());
            }

            r.m_defined = centroid(ys, r.m_output, r.m_crisp_output);
            return r;
        }

        InferenceResult Inference::runMulti(const std::vector<FuzzyInput>& in, Implication imp, Aggregation agg) const {
            InferenceResult r;
            r.m_implication = imp;
            r.m_aggregation = agg;
            r.m_algorithm = 3;

            const auto& names = m_kb.getMultiInputs();
            const auto& outVar = m_kb.getVariable(m_kb.getOutput());
            std::vector<double> ys = db::grid(outVar.m_universe);

            for (const auto& rule : m_kb.getMultiRules()) {
                std::vector<double> mu(names.size(), -1.0);
                double alpha = 1.0;
                for (const auto& cond : rule.m_condition) {
                    size_t j = std::find(names.begin(), names.end(), cond.m_var) - names.begin();
                    const auto& var = m_kb.getVariable(cond.m_var);
                    const db::Term* t = db::findTerm(var, cond.m_term);
                    // max_x min(A′_j, A_ij); для чёткого входа — A_ij(x_j0)
                    mu[j] = in[j].m_crisp ? db::membership(*t, var.m_universe, in[j].m_value)
                                          : height(in[j].m_set, db::termValues(*t, var.m_universe));
                    alpha = std::min(alpha, mu[j]);
                }
                r.m_input_mu.push_back(mu);
                r.m_alpha.push_back(alpha);

                FuzzySet B = db::termValues(*db::findTerm(outVar, rule.m_result.m_term), outVar.m_universe);
                FuzzySet out(ys.size());
                for (size_t y = 0; y < ys.size(); y++)
                    out[y] = implicate(imp, alpha, B[y]);
                r.m_rule_outputs.push_back(out);
            }

            r.m_output = aggregateSets(r.m_rule_outputs, agg, ys.size());
            r.m_defined = centroid(ys, r.m_output, r.m_crisp_output);
            return r;
        }

        std::string Inference::setToString(const std::string& var, const FuzzySet& A) const {
            std::vector<double> xs = db::grid(m_kb.getVariable(var).m_universe);
            bool all = xs.size() <= 5;   // короткие универсумы (лекция) печатаем целиком
            std::string res;
            for (size_t i = 0; i < xs.size(); i++) {
                if (!all && A[i] < 1e-9)
                    continue;
                if (!res.empty())
                    res += " + ";
                res += fmtTrim(xs[i], 2) + "/" + fmtTrim(A[i], 3);
            }
            return res.empty() ? "пустое множество (все степени 0)" : res;
        }

        void Inference::printMatrix(const Matrix& R, const std::vector<double>& xs, const std::vector<double>& ys) const {
            std::string line = pad("x \\ y", 8);
            for (double y : ys)
                line += pad(fmtTrim(y, 2), 6);
            std::cout << CYAN << line << RESET << "\n";
            for (size_t x = 0; x < xs.size(); x++) {
                line = pad(fmtTrim(xs[x], 2), 8);
                for (size_t y = 0; y < ys.size(); y++)
                    line += pad(fmtTrim(R[x][y], 2), 6);
                std::cout << line << "\n";
            }
        }

        // выходы правил: показываем те, что меняют итог агрегации, и говорим, какие скрыты
        void Inference::printOutputs(const InferenceResult& r, const std::vector<db::FuzzyRule>& rules) const {
            std::vector<double> ys = db::grid(m_kb.getVariable(m_kb.getOutput()).m_universe);
            std::cout << YELLOW << BOLD << "ВЫХОДЫ ПРАВИЛ B′_i" << RESET << "\n";
            std::string line = pad("y", 8);
            for (double y : ys)
                line += pad(fmtTrim(y, 2), 7);
            std::cout << CYAN << line << RESET << "\n";

            std::string hidden;
            for (size_t i = 0; i < rules.size(); i++) {
                if (isNeutral(r.m_rule_outputs[i], r.m_aggregation)) {
                    hidden += (hidden.empty() ? "R" : ", R") + std::to_string(rules[i].m_id);
                    continue;
                }
                line = pad("B′" + std::to_string(rules[i].m_id), 8);
                for (double v : r.m_rule_outputs[i])
                    line += pad(fmtTrim(v, 3), 7);   // три знака: 0,375 не должно превращаться в 0,38
                std::cout << line << "\n";
            }
            if (!hidden.empty())
                std::cout << "(не показаны " << hidden << ": их B′_i ≡ " << fmtTrim(neutral(r.m_aggregation), 2) << " —\n"
                          << " нейтральный элемент агрегации «" << toString(r.m_aggregation) << "», на итог не влияют)\n";
        }

        void Inference::printResult(const InferenceResult& r) const {
            std::vector<double> ys = db::grid(m_kb.getVariable(m_kb.getOutput()).m_universe);
            std::cout << YELLOW << BOLD << "ИТОГОВЫЙ ВЫХОД" << RESET << "\n";
            std::cout << "B′ = " << setToString(m_kb.getOutput(), r.m_output) << "\n";
            if (r.m_defined) {
                std::cout << GREEN << BOLD << "y* = " << fmt(r.m_crisp_output, 3) << RESET
                          << " (центр тяжести, " << m_kb.getVariable(m_kb.getOutput()).m_unit << ")\n";
                std::cout << "Ближайший терм: " << conclusion(r) << "\n";
                std::cout << GREEN << BOLD << "ЗАКЛЮЧЕНИЕ: " << m_kb.getOutput() << " — "
                          << nearestTerm(r.m_crisp_output).first << RESET << "\n";
            } else {
                std::cout << RED << BOLD << "B′ пустое — центр тяжести y* не определён" << RESET << "\n";
            }
        }

        // терм выхода с наибольшей степенью принадлежности в точке y (по параметрам терма);
        // если y на границе двух термов (степени равны), возвращаются оба через « / »
        std::pair<std::string, double> Inference::nearestTerm(double y) const {
            const auto& var = m_kb.getVariable(m_kb.getOutput());
            double best = 0.0;
            for (const auto& t : var.m_terms)
                best = std::max(best, db::membership(t, var.m_universe, y));
            std::string name;
            for (const auto& t : var.m_terms)
                if (best - db::membership(t, var.m_universe, y) < 0.005)   // равные с точностью до округления
                    name += (name.empty() ? "" : " / ") + t.m_name;
            return {name, best};
        }

        // «повышенный (μ = 0,81), соседний — средний (μ = 0,19)»
        std::string Inference::conclusion(const InferenceResult& r) const {
            if (!r.m_defined)
                return "не определён (B′ пустое)";
            auto best = nearestTerm(r.m_crisp_output);
            if (best.first.find(" / ") != std::string::npos)
                return best.first + " (μ = " + fmtTrim(best.second, 2) + ", на границе термов)";
            const auto& var = m_kb.getVariable(m_kb.getOutput());
            std::string second;
            double secondMu = 0.0;
            for (const auto& t : var.m_terms) {
                double mu = db::membership(t, var.m_universe, r.m_crisp_output);
                if (t.m_name != best.first && mu > secondMu) {
                    second = t.m_name;
                    secondMu = mu;
                }
            }
            std::string res = best.first + " (μ = " + fmtTrim(best.second, 2) + ")";
            if (secondMu > 1e-9)
                res += ", соседний — " + second + " (μ = " + fmtTrim(secondMu, 2) + ")";
            return res;
        }

        // три механизма рядом при текущей импликации и агрегации
        void Inference::printSummary(const FuzzyInput& in, Implication imp, Aggregation agg) const {
            std::vector<double> ys = db::grid(m_kb.getVariable(m_kb.getOutput()).m_universe);
            std::cout << YELLOW << BOLD << "СВОДКА ПО МЕХАНИЗМАМ (" << toString(imp) << ", " << toString(agg) << ")"
                      << RESET << "\n";
            std::string line = pad("y", 7);
            for (double y : ys)
                line += pad(fmtTrim(y, 2), 6);
            std::cout << CYAN << line << pad("y*", 5) << "терм" << RESET << "\n";
            for (int alg = 1; alg <= 3; alg++) {
                InferenceResult r = runSingle(in, imp, agg, alg);
                line = pad("Алг. " + std::to_string(alg), 7);
                for (double v : r.m_output)
                    line += pad(fmtTrim(v, 3), 6);
                line += pad(r.m_defined ? fmt(r.m_crisp_output, 2) : "—", 5);
                line += r.m_defined ? nearestTerm(r.m_crisp_output).first : "не определён";
                std::cout << line << "\n";
            }
        }

        void Inference::printSingle(const InferenceResult& r, const FuzzyInput& in) const {
            const auto& rules = m_kb.getSingleRules();
            const auto& inVar = m_kb.getVariable(m_kb.getSingleInput());
            std::vector<double> xs = db::grid(inVar.m_universe);
            std::vector<double> ys = db::grid(m_kb.getVariable(m_kb.getOutput()).m_universe);

            std::cout << CYAN << BOLD << "\n=== Вывод: один вход ===" << RESET << "\n"
                      << "Импликация: " << toString(r.m_implication)
                      << (isLogical(r.m_implication) ? " (логическая)" : " (конъюнктивная)")
                      << ", агрегация: " << toString(r.m_aggregation) << ", композиция: max-min\n"
                      << "Механизм: " << algorithmName(r.m_algorithm) << "\n";

            if (in.m_crisp)
                std::cout << "Вход: " << m_kb.getSingleInput() << " = " << fmtTrim(in.m_value, 3) << " (чёткий)\n";
            else
                std::cout << "Вход A′ (" << in.m_label << "): " << setToString(in.m_var, in.m_set) << "\n";
            if (r.m_snapped)
                std::cout << YELLOW << "Внимание: синглтон определён только в узлах сетки, для композиции вход "
                          << fmtTrim(in.m_value, 3) << " заменён ближайшим узлом " << fmtTrim(r.m_snapped_x, 3)
                          << RESET << "\n";

            std::cout << YELLOW << BOLD << "УРОВНИ ИСТИННОСТИ ПРЕДПОСЫЛОК" << RESET << "\n";
            for (size_t i = 0; i < rules.size(); i++) {
                std::cout << (r.m_alpha[i] > 1e-12 ? GREEN : "") << "α" << rules[i].m_id << " = "
                          << pad(fmtTrim(r.m_alpha[i], 3), 6) << RESET << "R" << rules[i].m_id << ": "
                          << rules[i].m_condition[0].m_term << " → " << rules[i].m_result.m_term << "\n";   // полностью правила — пункт 3
            }

            if (r.m_algorithm != 3) {
                std::cout << YELLOW << BOLD << "МАТРИЦЫ ПРАВИЛ R_i = A_i → B_i" << RESET << "\n";
                std::string skipped;
                for (size_t i = 0; i < rules.size(); i++) {
                    if (r.m_alpha[i] < 1e-12) {
                        skipped += (skipped.empty() ? "R" : ", R") + std::to_string(rules[i].m_id);
                        continue;
                    }
                    std::cout << "R" << rules[i].m_id << " (" << rules[i].m_condition[0].m_term << " → "
                              << rules[i].m_result.m_term << "):\n";
                    printMatrix(r.m_relations[i], xs, ys);
                }
                if (!skipped.empty())
                    std::cout << "(матрицы " << skipped << " не показаны: вход их не задевает, α_i = 0;\n"
                              << " в агрегации они всё равно участвуют)\n";
            }

            if (r.m_algorithm == 2) {
                std::cout << YELLOW << BOLD << "АГРЕГИРОВАННАЯ МАТРИЦА R = Agg(R_1, …, R_n)" << RESET << "\n";
                printMatrix(r.m_aggregated, xs, ys);
            } else {
                printOutputs(r, rules);
            }
            printResult(r);
        }

        void Inference::printMulti(const InferenceResult& r, const std::vector<FuzzyInput>& in) const {
            const auto& rules = m_kb.getMultiRules();
            const auto& names = m_kb.getMultiInputs();

            std::cout << CYAN << BOLD << "\n=== Вывод: несколько входов ===" << RESET << "\n"
                      << "Импликация: " << toString(r.m_implication)
                      << (isLogical(r.m_implication) ? " (логическая)" : " (конъюнктивная)")
                      << ", агрегация: " << toString(r.m_aggregation) << "\n"
                      << "Механизм: " << algorithmName(3) << "\n";
            for (size_t j = 0; j < names.size(); j++) {
                std::cout << "x" << j + 1 << " " << names[j] << " = ";
                if (in[j].m_crisp)
                    std::cout << fmtTrim(in[j].m_value, 3) << " (чёткий)\n";
                else
                    std::cout << setToString(names[j], in[j].m_set) << "\n";
            }

            std::cout << YELLOW << BOLD << "СТЕПЕНИ СООТВЕТСТВИЯ ВХОДОВ И УРОВНИ ИСТИННОСТИ" << RESET << "\n";
            for (size_t i = 0; i < rules.size(); i++) {
                std::string line = "R" + std::to_string(rules[i].m_id) + ": ";
                for (size_t c = 0; c < rules[i].m_condition.size(); c++) {
                    const auto& cond = rules[i].m_condition[c];
                    size_t j = std::find(names.begin(), names.end(), cond.m_var) - names.begin();
                    if (c > 0)
                        line += ", ";
                    line += cond.m_term + "(x" + std::to_string(j + 1) + ") = " + fmtTrim(r.m_input_mu[i][j], 3);
                }
                std::cout << (r.m_alpha[i] > 1e-12 ? GREEN : "") << pad(line, 72) << " α" << rules[i].m_id
                          << " = " << fmtTrim(r.m_alpha[i], 3) << RESET << "\n";
            }

            printOutputs(r, rules);
            printResult(r);
        }
    }
