#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "engine/Report.hpp"
#include "engine/Colors.hpp"

namespace engine {

        struct Style {
            std::string m_color;
            std::string m_dash;
            std::string m_marker;
        };

        // постоянный стиль импликации на всех графиках: цвет + штрих + маркер (различимо в ч/б)
        static Style implStyle(Implication imp) {
            switch (imp) {
                case Implication::Mamdani:      return {"#2a78d6", "", "circle"};
                case Implication::Larsen:       return {"#eb6834", "9,4", "square"};
                case Implication::Lukasiewicz:  return {"#1baf7a", "2,4", "triangle"};
                case Implication::Godel:        return {"#eda100", "10,4,2,4", "diamond"};
                case Implication::KleeneDienes: return {"#e87ba4", "5,3", "cross"};
            }
            return {"#000000", "", "none"};
        }

        static const std::vector<std::string> termColors = {
            "#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6"
        };
        static const std::vector<std::string> termDashes = {"", "7,4", "2,3"};
        static const std::vector<std::string> termMarkers = {"circle", "square", "triangle", "diamond", "cross"};
        static const std::vector<std::string> ruleDashes = {"5,3", "2,3", "9,3,2,3", "12,4"};
        static const char* GRAY = "#52514e";
        static const double NaN = std::numeric_limits<double>::quiet_NaN();

        static std::string axisLabel(const db::LinguisticVariable& var) {
            return var.m_unit.empty() ? var.m_title : var.m_title + ", " + var.m_unit;
        }

        static std::string groupLabel(Implication imp) {
            return toString(imp) + "\n(Agg = " + toString(defaultAggregation(imp)) + ")";
        }

        // подписи по x: узлы сетки, если их немного
        static std::vector<double> gridTicks(const db::Universe& u) {
            std::vector<double> xs = db::grid(u);
            if (xs.size() <= 25)
                return xs;
            return ticks(u.m_min, u.m_max, (u.m_max - u.m_min) / 10);
        }

        const std::vector<std::string>& Report::getFiles() const {
            return m_files;
        }

        bool Report::save(const SvgFigure& fig, const std::string& dir, const std::string& name) {
            std::string path = dir + "/" + name;
            if (!fig.save(path)) {
                std::cout << RED << "Не удалось записать " << path << RESET << "\n";
                return false;
            }
            m_files.push_back(path);
            return true;
        }

        Panel Report::termsPanel(const std::string& name) const {
            const auto& var = m_kb.getVariable(name);
            std::vector<double> xs = db::grid(var.m_universe);
            Panel p;
            p.m_title = var.m_title;
            p.m_xlabel = axisLabel(var);
            p.m_ylabel = "Степень принадлежности μ";
            p.m_xmin = xs.front();
            p.m_xmax = xs.back();
            p.m_ymax = 1.05;
            p.m_xticks = gridTicks(var.m_universe);
            p.m_yticks = ticks(0, 1, 0.25);
            for (size_t i = 0; i < var.m_terms.size(); i++) {
                Series s;
                s.m_label = display(var.m_terms[i].m_name);
                s.m_x = xs;
                s.m_y = db::termValues(var.m_terms[i], var.m_universe);
                s.m_color = termColors[i % termColors.size()];
                s.m_dash = termDashes[i % termDashes.size()];
                s.m_marker = termMarkers[i % termMarkers.size()];
                p.m_series.push_back(s);
            }
            return p;
        }

        // итоговое B′ толстой полупрозрачной линией, выходы сработавших правил — тонкими серыми поверх
        Panel Report::outputPanel(const InferenceResult& r, const std::vector<db::FuzzyRule>& rules, const std::string& title) const {
            const auto& var = m_kb.getVariable(m_kb.getOutput());
            std::vector<double> ys = db::grid(var.m_universe);
            Style st = implStyle(r.m_implication);
            Panel p;
            p.m_title = title;
            p.m_xlabel = axisLabel(var);
            p.m_ylabel = "Степень принадлежности μ";
            p.m_xmin = ys.front();
            p.m_xmax = ys.back();
            p.m_ymax = 1.05;
            p.m_xticks = gridTicks(var.m_universe);
            p.m_yticks = ticks(0, 1, 0.25);

            Series total;
            total.m_label = "B′ — итоговый выход (" + toString(r.m_implication) + ")";
            total.m_x = ys;
            total.m_y = r.m_output;
            total.m_color = st.m_color;
            total.m_marker = st.m_marker;
            total.m_width = 6;
            total.m_opacity = 0.45;
            p.m_series.push_back(total);

            int k = 0;
            bool hiddenNonNeutral = false;
            for (size_t i = 0; i < r.m_rule_outputs.size(); i++) {
                if (r.m_alpha[i] < 1e-12) {
                    if (!isNeutral(r.m_rule_outputs[i], r.m_aggregation))
                        hiddenNonNeutral = true;
                    continue;
                }
                Series s;
                s.m_label = "B′" + std::to_string(rules[i].m_id) + " — выход R" + std::to_string(rules[i].m_id)
                          + " (α = " + fmtTrim(r.m_alpha[i], 2) + ")";
                s.m_x = ys;
                s.m_y = r.m_rule_outputs[i];
                s.m_color = GRAY;
                s.m_dash = ruleDashes[k % ruleDashes.size()];
                s.m_marker = "none";
                s.m_width = 1.8;
                p.m_series.push_back(s);
                k++;
            }
            if (hiddenNonNeutral)
                p.m_notes.push_back("Показаны выходы правил с αᵢ > 0; остальные правила тоже участвуют в агрегации.");
            if (r.m_defined)
                p.m_vlines.push_back({r.m_crisp_output, "y* = " + fmt(r.m_crisp_output, 2), st.m_color});
            else
                p.m_notes.push_back("B′ пустое — центр тяжести y* не определён.");
            return p;
        }

        Panel Report::sweepPanel(const std::string& name, const std::vector<Implication>& list,
                                 const std::vector<std::vector<double>>& values, const std::vector<double>& xs,
                                 const std::string& title) const {
            const auto& var = m_kb.getVariable(name);
            const auto& out = m_kb.getVariable(m_kb.getOutput());
            Panel p;
            p.m_title = title;
            p.m_xlabel = axisLabel(var);
            p.m_ylabel = "y*, " + out.m_unit;
            p.m_xmin = xs.front();
            p.m_xmax = xs.back();
            p.m_ymin = out.m_universe.m_min;
            p.m_ymax = out.m_universe.m_max;
            p.m_xticks = gridTicks(var.m_universe);
            p.m_yticks = ticks(out.m_universe.m_min, out.m_universe.m_max, (out.m_universe.m_max - out.m_universe.m_min) / 6);
            bool gaps = false;
            for (size_t i = 0; i < list.size(); i++) {
                Style st = implStyle(list[i]);
                Series s;
                s.m_label = toString(list[i]);
                s.m_x = xs;
                s.m_y = values[i];
                s.m_color = st.m_color;
                s.m_dash = st.m_dash;
                s.m_marker = st.m_marker;
                s.m_marker_every = std::max(1, (int)xs.size() / 12);
                p.m_series.push_back(s);
                for (double v : values[i])
                    if (std::isnan(v)) gaps = true;
            }
            if (gaps)
                p.m_notes.push_back("Разрывы линий: B′ пустое, y* не определён.");
            return p;
        }

        bool Report::saveInferencePlot(const InferenceResult& r, const FuzzyInput& in, bool multi, const std::string& path) const {
            const auto& rules = multi ? m_kb.getMultiRules() : m_kb.getSingleRules();
            std::string title = toString(r.m_implication) + ", Agg = " + toString(r.m_aggregation) + ", "
                              + (multi ? "несколько входов" : "алгоритм " + std::to_string(r.m_algorithm))
                              + (multi ? "" : ", вход: " + in.m_label);
            SvgFigure fig(900, 600, "", 1, 1);
            fig.setPanel(0, 0, outputPanel(r, rules, title));
            return fig.save(path);
        }

        bool Report::generate(const std::string& dir) {
            m_files.clear();
            std::filesystem::create_directories(dir);
            const db::Scenario* single = m_kb.findScenario("single");
            const db::Scenario* multi = m_kb.findScenario("multi");
            if (single == nullptr || multi == nullptr) {
                std::cout << RED << "В базе нет сценариев single и multi — отчёт не построен" << RESET << "\n";
                return false;
            }
            FuzzyInput in = m_inference.makeInputs(*single)[0];
            std::vector<FuzzyInput> mins = m_inference.makeInputs(*multi);
            const auto& inVar = m_kb.getVariable(m_kb.getSingleInput());
            const auto& outVar = m_kb.getVariable(m_kb.getOutput());
            const auto& names = m_kb.getMultiInputs();
            bool ok = true;

            // 1. термы входа и нечёткий вход A′
            {
                Panel p = termsPanel(m_kb.getSingleInput());
                p.m_title = "";
                Series a;
                a.m_label = "A′ — вход «" + in.m_label + "»";
                a.m_x = db::grid(inVar.m_universe);
                a.m_y = in.m_set;
                a.m_color = "#0b0b0b";
                a.m_marker = "square";
                a.m_width = 4;
                p.m_series.push_back(a);
                SvgFigure fig(900, 600, "Термы входной переменной «" + inVar.m_title + "» и нечёткий вход A′", 1, 1);
                fig.setPanel(0, 0, p);
                ok = save(fig, dir, "01_input_terms.svg") && ok;
            }

            // 2. термы выхода
            {
                Panel p = termsPanel(m_kb.getOutput());
                p.m_title = "";
                SvgFigure fig(900, 560, "Термы выходной переменной «" + outVar.m_title + "»", 1, 1);
                fig.setPanel(0, 0, p);
                ok = save(fig, dir, "02_output_terms.svg") && ok;
            }

            // 3. термы входов системы с несколькими входами
            {
                SvgFigure fig(900, 395 * names.size() + 50, "Термы входных переменных системы с несколькими входами",
                              1, names.size());
                for (size_t j = 0; j < names.size(); j++) {
                    Panel p = termsPanel(names[j]);
                    p.m_title = "x" + std::to_string(j + 1) + ": " + p.m_title;
                    fig.setPanel(0, j, p);
                }
                ok = save(fig, dir, "03_multi_input_terms.svg") && ok;
            }

            // результаты п. 2: все импликации × три алгоритма, агрегация по умолчанию
            std::vector<std::vector<InferenceResult>> res;
            for (auto imp : allImplications) {
                std::vector<InferenceResult> row;
                for (int alg = 1; alg <= 3; alg++)
                    row.push_back(m_inference.runSingle(in, imp, defaultAggregation(imp), alg));
                res.push_back(row);
            }

            // 4. Мамдани и Ларсен, алгоритм 1
            {
                const char* files[] = {"04a_single_mamdani.svg", "04b_single_larsen.svg"};
                for (int k = 0; k < 2; k++) {
                    const auto& r = res[k][0];
                    SvgFigure fig(900, 640, toString(r.m_implication) + ", алгоритм 1, Agg = max: выходы правил и итоговое B′"
                                  + " (вход «" + in.m_label + "»)", 1, 1);
                    fig.setPanel(0, 0, outputPanel(r, m_kb.getSingleRules(), ""));
                    ok = save(fig, dir, files[k]) && ok;
                }
            }

            // 5. сравнение импликаций: отдельно конъюнктивные и логические, панели — алгоритмы 1–3 друг под другом
            for (int logical = 0; logical <= 1; logical++) {
                std::vector<double> ys = db::grid(outVar.m_universe);
                SvgFigure fig(900, 1240, std::string("Итоговые B′: ") + (logical ? "логические импликации, Agg = min"
                              : "конъюнктивные импликации, Agg = max"), 1, 3);
                {
                    for (int alg = 1; alg <= 3; alg++) {
                        Panel p;
                        p.m_title = "Алгоритм " + std::to_string(alg);
                        p.m_xlabel = axisLabel(outVar);
                        p.m_ylabel = "Степень принадлежности μ";
                        p.m_xmin = ys.front();
                        p.m_xmax = ys.back();
                        p.m_xticks = gridTicks(outVar.m_universe);
                        p.m_yticks = ticks(0, 1, 0.25);
                        std::vector<size_t> group;
                        for (size_t i = 0; i < allImplications.size(); i++)
                            if (isLogical(allImplications[i]) == (logical == 1))
                                group.push_back(i);
                        for (size_t k = 0; k < group.size(); k++) {
                            Implication imp = allImplications[group[k]];
                            const auto& r = res[group[k]][alg - 1];
                            Style st = implStyle(imp);
                            Series s;
                            s.m_label = toString(imp) + (r.m_defined ? ", y* = " + fmt(r.m_crisp_output, 2) : ", y* не определён");
                            s.m_x = ys;
                            s.m_y = r.m_output;
                            s.m_color = st.m_color;
                            s.m_dash = st.m_dash;
                            s.m_marker = st.m_marker;
                            // нижние линии шире: при совпадении верхняя не закрывает их целиком
                            s.m_width = 2.0 + 2.5 * (group.size() - 1 - k);
                            p.m_series.push_back(s);
                            if (r.m_defined)
                                p.m_vlines.push_back({r.m_crisp_output, fmt(r.m_crisp_output, 2), st.m_color});
                            // пустое B′ отмечено в легенде: «y* не определён»
                        }
                        p.m_ymax = 1.05;
                        fig.setPanel(0, alg - 1, p);
                    }
                }
                ok = save(fig, dir, logical ? "05b_single_compare_logic.svg" : "05a_single_compare_conj.svg") && ok;
            }

            // 6. столбчатая диаграмма y*
            {
                Panel p;
                p.m_ylabel = "y*, " + outVar.m_unit;
                p.m_ymin = outVar.m_universe.m_min;
                p.m_ymax = outVar.m_universe.m_max * 1.1;
                p.m_yticks = ticks(outVar.m_universe.m_min, outVar.m_universe.m_max, (outVar.m_universe.m_max - outVar.m_universe.m_min) / 6);
                const char* fills[] = {"solid", "hatch", "light"};
                for (size_t i = 0; i < allImplications.size(); i++)
                    for (int alg = 0; alg < 3; alg++) {
                        const auto& r = res[i][alg];
                        p.m_bars.push_back({groupLabel(allImplications[i]), std::to_string(alg + 1), r.m_crisp_output,
                                            r.m_defined, implStyle(allImplications[i]).m_color, fills[alg]});
                    }
                for (int alg = 0; alg < 3; alg++)
                    p.m_legend.push_back({"алгоритм " + std::to_string(alg + 1), GRAY, "", "", fills[alg]});
                SvgFigure fig(900, 600, "y* при разных импликациях и механизмах вывода (вход «" + in.m_label + "»)", 1, 1);
                fig.setPanel(0, 0, p);
                ok = save(fig, dir, "06_single_ystar_bars.svg") && ok;
            }

            // 7. характеристика y*(x) при чётком входе
            {
                std::vector<double> xs;
                double step = (inVar.m_universe.m_max - inVar.m_universe.m_min) / 120;
                for (int k = 0; k <= 120; k++)
                    xs.push_back(inVar.m_universe.m_min + k * step);
                std::vector<Implication> conj = {Implication::Mamdani, Implication::Larsen};
                std::vector<Implication> logic = {Implication::Lukasiewicz, Implication::Godel, Implication::KleeneDienes};
                SvgFigure fig(900, 1180, "Характеристика системы с одним входом: y*(x), вход чёткий", 1, 2);
                for (int part = 0; part < 2; part++) {
                    const auto& list = part == 0 ? conj : logic;
                    std::vector<std::vector<double>> values;
                    for (auto imp : list) {
                        std::vector<double> v;
                        for (double x : xs) {
                            FuzzyInput c;
                            c.m_var = m_kb.getSingleInput();
                            c.m_crisp = true;
                            c.m_value = x;
                            InferenceResult r = m_inference.runSingle(c, imp, defaultAggregation(imp), 3);
                            v.push_back(r.m_defined ? r.m_crisp_output : NaN);
                        }
                        values.push_back(v);
                    }
                    Panel p = sweepPanel(m_kb.getSingleInput(), list, values, xs,
                                         part == 0 ? "Конъюнктивные, Agg = max" : "Логические, Agg = min");
                    p.m_notes.insert(p.m_notes.begin(), "При чётком входе алгоритмы 1–3 совпадают (в узлах сетки); кривые — алгоритм 3, αᵢ = Aᵢ(x), шаг x = "
                                     + fmtTrim(step, 2) + ".");
                    fig.setPanel(0, part, p);
                }
                ok = save(fig, dir, "07_single_characteristic.svg") && ok;
            }

            // результаты п. 3
            std::vector<InferenceResult> mres;
            for (auto imp : allImplications)
                mres.push_back(m_inference.runMulti(mins, imp, defaultAggregation(imp)));

            // 8. уровни истинности и выходы Мамдани, Ларсена
            {
                std::string inputs;
                for (size_t j = 0; j < mins.size(); j++)
                    inputs += (j ? ", x" : "x") + std::to_string(j + 1) + " = " + mins[j].m_label;
                SvgFigure fig(900, 1210, "Система с несколькими входами (" + inputs + ")", 1, 3);
                Panel a;
                a.m_title = "Уровни истинности предпосылок αᵢ";
                a.m_ylabel = "αᵢ";
                a.m_ymax = 1.1;
                a.m_yticks = ticks(0, 1, 0.25);
                const auto& rules = m_kb.getMultiRules();
                for (size_t i = 0; i < rules.size(); i++)
                    a.m_bars.push_back({"R" + std::to_string(rules[i].m_id), "", mres[0].m_alpha[i], true, GRAY, "solid"});
                fig.setPanel(0, 0, a);
                fig.setPanel(0, 1, outputPanel(mres[0], rules, "Мамдани, Agg = max"));
                fig.setPanel(0, 2, outputPanel(mres[1], rules, "Ларсен, Agg = max"));
                ok = save(fig, dir, "08_multi_alpha_outputs.svg") && ok;
            }

            // 9. y* по импликациям
            {
                Panel p;
                p.m_ylabel = "y*, " + outVar.m_unit;
                p.m_ymin = outVar.m_universe.m_min;
                p.m_ymax = outVar.m_universe.m_max * 1.1;
                p.m_yticks = ticks(outVar.m_universe.m_min, outVar.m_universe.m_max, (outVar.m_universe.m_max - outVar.m_universe.m_min) / 6);
                for (size_t i = 0; i < allImplications.size(); i++)
                    p.m_bars.push_back({groupLabel(allImplications[i]), "", mres[i].m_crisp_output, mres[i].m_defined,
                                        implStyle(allImplications[i]).m_color, "solid"});
                SvgFigure fig(900, 560, "Система с несколькими входами: y* при разных импликациях", 1, 1);
                fig.setPanel(0, 0, p);
                ok = save(fig, dir, "09_multi_ystar_bars.svg") && ok;
            }

            // 10. y*(x1) при фиксированных остальных входах
            {
                const auto& v1 = m_kb.getVariable(names[0]);
                std::vector<double> xs;
                double step = (v1.m_universe.m_max - v1.m_universe.m_min) / 120;
                for (int k = 0; k <= 120; k++)
                    xs.push_back(v1.m_universe.m_min + k * step);
                std::string fixed;
                for (size_t j = 1; j < mins.size(); j++)
                    fixed += (j > 1 ? ", x" : "x") + std::to_string(j + 1) + " = " + mins[j].m_label;
                std::vector<Implication> conj = {Implication::Mamdani, Implication::Larsen};
                std::vector<Implication> logic = {Implication::Lukasiewicz, Implication::Godel, Implication::KleeneDienes};
                SvgFigure fig(900, 1100, "Система с несколькими входами: y*(x1) при " + fixed, 1, 2);
                for (int part = 0; part < 2; part++) {
                    const auto& list = part == 0 ? conj : logic;
                    std::vector<std::vector<double>> values;
                    for (auto imp : list) {
                        std::vector<double> v;
                        for (double x : xs) {
                            std::vector<FuzzyInput> cur = mins;
                            cur[0].m_crisp = true;
                            cur[0].m_value = x;
                            InferenceResult r = m_inference.runMulti(cur, imp, defaultAggregation(imp));
                            v.push_back(r.m_defined ? r.m_crisp_output : NaN);
                        }
                        values.push_back(v);
                    }
                    Panel p = sweepPanel(names[0], list, values, xs,
                                         part == 0 ? "Конъюнктивные, Agg = max" : "Логические, Agg = min");
                    p.m_xlabel = "x1: " + p.m_xlabel;
                    fig.setPanel(0, part, p);
                }
                ok = save(fig, dir, "10_multi_characteristic_x1.svg") && ok;
            }

            return writeTables(dir, *single, *multi) && ok;
        }

        // таблицы для записки: markdown и csv (разделитель «;», десятичная запятая)
        bool Report::writeTables(const std::string& dir, const db::Scenario& single, const db::Scenario& multi) {
            FuzzyInput in = m_inference.makeInputs(single)[0];
            std::vector<FuzzyInput> mins = m_inference.makeInputs(multi);
            const auto& inVar = m_kb.getVariable(m_kb.getSingleInput());
            const auto& outVar = m_kb.getVariable(m_kb.getOutput());
            std::vector<double> xs = db::grid(inVar.m_universe);
            std::vector<double> ys = db::grid(outVar.m_universe);
            const auto& srules = m_kb.getSingleRules();
            const auto& mrules = m_kb.getMultiRules();
            const auto& names = m_kb.getMultiInputs();
            std::ostringstream md, csv;

            md << "# Результаты расчётов (сформировано программой)\n\n"
               << "База знаний: `" << m_kb.getPath() << "`. Дефаззификация — центр тяжести. "
               << "Агрегация по умолчанию: max для конъюнктивных импликаций, min для логических.\n\n";

            // п. 2: вход и уровни истинности
            InferenceResult base = m_inference.runSingle(in, Implication::Mamdani, Aggregation::Max, 3);
            md << "## Система с одним входом\n\nВход A′ «" << in.m_label << "» = "
               << m_inference.setToString(in.m_var, in.m_set) << "\n\n"
               << "### Уровни истинности предпосылок\n\n| Правило | ЕСЛИ " << m_kb.getSingleInput() << " | ТО "
               << m_kb.getOutput() << " | α_i |\n|---|---|---|---|\n";
            csv << "Уровни истинности, один вход\nПравило;Терм входа;Терм выхода;alpha\n";
            for (size_t i = 0; i < srules.size(); i++) {
                md << "| R" << srules[i].m_id << " | " << srules[i].m_condition[0].m_term << " | "
                   << srules[i].m_result.m_term << " | " << fmtTrim(base.m_alpha[i], 3) << " |\n";
                csv << "R" << srules[i].m_id << ";" << srules[i].m_condition[0].m_term << ";"
                    << srules[i].m_result.m_term << ";" << fmtTrim(base.m_alpha[i], 4) << "\n";
            }

            // y* по импликациям и алгоритмам, агрегация по умолчанию
            md << "\n### y* по импликациям и механизмам (агрегация по умолчанию)\n\n"
               << "| Импликация | Agg | Алгоритм 1 | Алгоритм 2 | Алгоритм 3 |\n|---|---|---|---|---|\n";
            csv << "\ny*, один вход, агрегация по умолчанию\nИмпликация;Agg;Алгоритм 1;Алгоритм 2;Алгоритм 3\n";
            for (auto imp : allImplications) {
                md << "| " << toString(imp) << " | " << toString(defaultAggregation(imp));
                csv << toString(imp) << ";" << toString(defaultAggregation(imp));
                for (int alg = 1; alg <= 3; alg++) {
                    InferenceResult r = m_inference.runSingle(in, imp, defaultAggregation(imp), alg);
                    md << " | " << (r.m_defined ? fmt(r.m_crisp_output, 2) : "не определён (B′ пустое)");
                    csv << ";" << (r.m_defined ? fmt(r.m_crisp_output, 4) : "не определён");
                }
                md << " |\n";
                csv << "\n";
            }

            // все агрегации
            md << "\n### y* при всех операторах агрегации\n\n"
               << "| Импликация | Agg | Алгоритм 1 | Алгоритм 2 | Алгоритм 3 |\n|---|---|---|---|---|\n";
            csv << "\ny*, один вход, все агрегации\nИмпликация;Agg;Алгоритм 1;Алгоритм 2;Алгоритм 3\n";
            for (auto imp : allImplications)
                for (auto agg : allAggregations) {
                    md << "| " << toString(imp) << " | " << toString(agg);
                    csv << toString(imp) << ";" << toString(agg);
                    for (int alg = 1; alg <= 3; alg++) {
                        InferenceResult r = m_inference.runSingle(in, imp, agg, alg);
                        md << " | " << (r.m_defined ? fmt(r.m_crisp_output, 2) : "не опр.");
                        csv << ";" << (r.m_defined ? fmt(r.m_crisp_output, 4) : "не определён");
                    }
                    md << " |\n";
                    csv << "\n";
                }

            // B′ всех вариантов
            md << "\n### Итоговые B′ (агрегация по умолчанию)\n\n| Вариант |";
            csv << "\nB′, один вход\nВариант";
            for (double y : ys) {
                md << " " << fmtTrim(y, 2) << " |";
                csv << ";" << fmtTrim(y, 2);
            }
            md << " y* |\n|---|";
            csv << ";y*\n";
            for (size_t k = 0; k <= ys.size(); k++)
                md << "---|";
            md << "\n";
            for (auto imp : allImplications)
                for (int alg = 1; alg <= 3; alg++) {
                    InferenceResult r = m_inference.runSingle(in, imp, defaultAggregation(imp), alg);
                    md << "| " << toString(imp) << ", алг. " << alg << " |";
                    csv << toString(imp) << " алг. " << alg;
                    for (double v : r.m_output) {
                        md << " " << fmtTrim(v, 3) << " |";
                        csv << ";" << fmtTrim(v, 4);
                    }
                    md << " " << (r.m_defined ? fmt(r.m_crisp_output, 2) : "—") << " |\n";
                    csv << ";" << (r.m_defined ? fmt(r.m_crisp_output, 4) : "не определён") << "\n";
                }

            // пошаговый пример: Мамдани, алгоритм 1
            InferenceResult step = m_inference.runSingle(in, Implication::Mamdani, Aggregation::Max, 1);
            md << "\n### Пошаговый пример: Мамдани, алгоритм 1, Agg = max\n\n";
            for (size_t i = 0; i < srules.size(); i++) {
                if (step.m_alpha[i] < 1e-12)
                    continue;
                md << "Матрица R" << srules[i].m_id << " = " << srules[i].m_condition[0].m_term << " → "
                   << srules[i].m_result.m_term << " (строки — x, столбцы — y):\n\n| x \\ y |";
                for (double y : ys)
                    md << " " << fmtTrim(y, 2) << " |";
                md << "\n|---|";
                for (size_t k = 0; k < ys.size(); k++)
                    md << "---|";
                md << "\n";
                for (size_t x = 0; x < xs.size(); x++) {
                    md << "| " << fmtTrim(xs[x], 2) << " |";
                    for (double v : step.m_relations[i][x])
                        md << " " << fmtTrim(v, 2) << " |";
                    md << "\n";
                }
                md << "\nB′" << srules[i].m_id << " = A′∘R" << srules[i].m_id << " = "
                   << m_inference.setToString(m_kb.getOutput(), step.m_rule_outputs[i]) << "\n\n";
            }
            md << "B′ = max(B′_i) = " << m_inference.setToString(m_kb.getOutput(), step.m_output) << "\n\n"
               << "y* = " << fmt(step.m_crisp_output, 4) << "\n";

            // п. 3
            md << "\n## Система с несколькими входами\n\nВходы:";
            for (size_t j = 0; j < names.size(); j++)
                md << (j ? ", x" : " x") << j + 1 << " " << names[j] << " = " << mins[j].m_label;
            md << "\n\n### Степени соответствия и уровни истинности\n\n| Правило |";
            csv << "\nУровни истинности, несколько входов\nПравило";
            for (size_t j = 0; j < names.size(); j++) {
                md << " x" << j + 1 << " |";
                csv << ";x" << j + 1 << ";mu x" << j + 1;
            }
            md << " ТО | α_i |\n|---|";
            csv << ";ТО;alpha\n";
            for (size_t j = 0; j < names.size() + 2; j++)
                md << "---|";
            md << "\n";
            InferenceResult mbase = m_inference.runMulti(mins, Implication::Mamdani, Aggregation::Max);
            for (size_t i = 0; i < mrules.size(); i++) {
                md << "| R" << mrules[i].m_id << " |";
                csv << "R" << mrules[i].m_id;
                for (size_t j = 0; j < names.size(); j++) {
                    std::string term = "—";
                    for (const auto& c : mrules[i].m_condition)
                        if (c.m_var == names[j]) term = c.m_term;
                    double mu = mbase.m_input_mu[i][j];
                    md << " " << term << " (" << (mu < 0 ? "—" : fmtTrim(mu, 3)) << ") |";
                    csv << ";" << term << ";" << (mu < 0 ? "" : fmtTrim(mu, 4));
                }
                md << " " << mrules[i].m_result.m_term << " | " << fmtTrim(mbase.m_alpha[i], 3) << " |\n";
                csv << ";" << mrules[i].m_result.m_term << ";" << fmtTrim(mbase.m_alpha[i], 4) << "\n";
            }

            md << "\n### y* по импликациям (алгоритм 3, агрегация по умолчанию)\n\n| Импликация | Agg |";
            csv << "\nНесколько входов, алгоритм 3\nИмпликация;Agg";
            for (double y : ys) {
                md << " " << fmtTrim(y, 2) << " |";
                csv << ";" << fmtTrim(y, 2);
            }
            md << " y* |\n|---|---|";
            csv << ";y*\n";
            for (size_t k = 0; k <= ys.size(); k++)
                md << "---|";
            md << "\n";
            for (auto imp : allImplications) {
                InferenceResult r = m_inference.runMulti(mins, imp, defaultAggregation(imp));
                md << "| " << toString(imp) << " | " << toString(defaultAggregation(imp)) << " |";
                csv << toString(imp) << ";" << toString(defaultAggregation(imp));
                for (double v : r.m_output) {
                    md << " " << fmtTrim(v, 3) << " |";
                    csv << ";" << fmtTrim(v, 4);
                }
                md << " " << (r.m_defined ? fmt(r.m_crisp_output, 3) : "не определён") << " |\n";
                csv << ";" << (r.m_defined ? fmt(r.m_crisp_output, 4) : "не определён") << "\n";
            }

            // словесный итог: терм выхода, ближайший к y*
            md << "\n## Словесный итог (терм выхода с наибольшей степенью в точке y*)\n\n"
               << "| Импликация | Agg | Алгоритм 1 | Алгоритм 2 | Алгоритм 3 | Три входа |\n|---|---|---|---|---|---|\n";
            csv << "\nСловесный итог\nИмпликация;Agg;Алгоритм 1;Алгоритм 2;Алгоритм 3;Три входа\n";
            for (auto imp : allImplications) {
                md << "| " << toString(imp) << " | " << toString(defaultAggregation(imp));
                csv << toString(imp) << ";" << toString(defaultAggregation(imp));
                for (int alg = 1; alg <= 3; alg++) {
                    std::string c = m_inference.conclusion(m_inference.runSingle(in, imp, defaultAggregation(imp), alg));
                    md << " | " << c;
                    csv << ";" << c;
                }
                std::string c = m_inference.conclusion(m_inference.runMulti(mins, imp, defaultAggregation(imp)));
                md << " | " << c << " |\n";
                csv << ";" << c << "\n";
            }

            // характеристики в узлах сетки
            md << "\n## Характеристики при чётком входе (узлы сетки)\n\n| " << m_kb.getSingleInput() << " |";
            csv << "\nХарактеристика y*(x), один вход\nx";
            for (auto imp : allImplications) {
                md << " " << toString(imp) << " |";
                csv << ";" << toString(imp);
            }
            md << "\n|---|";
            csv << "\n";
            for (size_t k = 0; k < allImplications.size(); k++)
                md << "---|";
            md << "\n";
            for (double x : xs) {
                md << "| " << fmtTrim(x, 2) << " |";
                csv << fmtTrim(x, 2);
                for (auto imp : allImplications) {
                    FuzzyInput c;
                    c.m_var = m_kb.getSingleInput();
                    c.m_crisp = true;
                    c.m_value = x;
                    InferenceResult r = m_inference.runSingle(c, imp, defaultAggregation(imp), 3);
                    md << " " << (r.m_defined ? fmt(r.m_crisp_output, 2) : "—") << " |";
                    csv << ";" << (r.m_defined ? fmt(r.m_crisp_output, 4) : "");
                }
                md << "\n";
                csv << "\n";
            }

            std::ofstream fmd(dir + "/results.md"), fcsv(dir + "/results.csv");
            if (!fmd.is_open() || !fcsv.is_open()) {
                std::cout << RED << "Не удалось записать таблицы результатов" << RESET << "\n";
                return false;
            }
            fmd << md.str();
            fcsv << csv.str();
            m_files.push_back(dir + "/results.md");
            m_files.push_back(dir + "/results.csv");
            return true;
        }
    }
