#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "db/KnowledgeBase.hpp"
#include "engine/FuzzyMath.hpp"
#include "engine/Inference.hpp"
#include "engine/Report.hpp"
#include "engine/SelfTest.hpp"
#include "engine/ReadChoice.hpp"
#include "engine/Colors.hpp"

// Чтение вещественного числа из диапазона; принимается и точка, и запятая.
double readDouble(double min, double max) {
    while (true) {
        std::cout << GREEN << "Введите число от " << engine::fmtTrim(min, 3) << " до "
                  << engine::fmtTrim(max, 3) << ": " << RESET;
        std::string line;
        std::getline(std::cin, line);
        for (auto& c : line)
            if (c == ',') c = '.';
        try {
            size_t used = 0;
            double value = std::stod(line, &used);
            if (used == line.size() && value >= min && value <= max)
                return value;
        } catch (...) {}
        std::cout << RED << "Ожидается число от " << engine::fmtTrim(min, 3) << " до "
                  << engine::fmtTrim(max, 3) << RESET << "\n";
    }
}

// Выбор терма переменной. Возвращает индекс терма.
int readTerm(const db::LinguisticVariable& var) {
    std::cout << "Выберите терм:\n";
    for (size_t i = 0; i < var.m_terms.size(); i++)
        std::cout << i + 1 << ". " << var.m_terms[i].m_name << "\n";
    return readChoice(1, var.m_terms.size()) - 1;
}

// Ввод значения одной входной переменной: число, терм, терм с модификатором или по точкам.
engine::FuzzyInput readInput(const db::KnowledgeBase& kb, const engine::Inference& inference, const std::string& varName) {
    const auto& var = kb.getVariable(varName);
    std::vector<double> xs = db::grid(var.m_universe);
    std::cout << YELLOW << BOLD << "ВХОД: " << varName << RESET << " (" << var.m_unit << ")\n"
              << "1. Чёткое число\n"
              << "2. Терм из базы\n"
              << "3. Терм с модификатором\n"
              << "4. Значения по точкам\n";
    db::InputSpec spec;
    spec.m_var = varName;
    switch (readChoice(1, 4)) {
        case 1: {
            spec.m_kind = "crisp";
            spec.m_value = readDouble(var.m_universe.m_min, var.m_universe.m_max);
            break;
        }
        case 2: {
            spec.m_kind = "term";
            spec.m_term = var.m_terms[readTerm(var)].m_name;
            break;
        }
        case 3: {
            spec.m_kind = "term";
            spec.m_term = var.m_terms[readTerm(var)].m_name;
            std::cout << "Модификатор:\n"
                      << "1. очень (A²)\n"
                      << "2. более-менее (√A)\n";
            spec.m_modifier = readChoice(1, 2) == 1 ? "очень" : "более_менее";
            break;
        }
        case 4: {
            spec.m_kind = "points";
            std::cout << "Степень принадлежности в каждой точке сетки:\n";
            for (double x : xs) {
                std::cout << "x = " << engine::fmtTrim(x, 3) << ". ";
                spec.m_params.push_back(readDouble(0, 1));
            }
            break;
        }
    }
    engine::FuzzyInput in = inference.makeInput(spec);
    if (!in.m_crisp)
        std::cout << "A′ = " << inference.setToString(varName, in.m_set) << "\n";
    return in;
}

// Выбор готового сценария системы ("single" или "multi"); nullptr, если сценариев нет.
const db::Scenario* readScenario(const db::KnowledgeBase& kb, const std::string& system) {
    std::vector<const db::Scenario*> list;
    for (const auto& s : kb.getScenarios())
        if (s.m_system == system)
            list.push_back(&s);
    if (list.empty()) {
        std::cout << YELLOW << "В базе нет сценариев для этой системы" << RESET << "\n";
        return nullptr;
    }
    std::cout << "Выберите сценарий:\n";
    for (size_t i = 0; i < list.size(); i++)
        std::cout << i + 1 << ". " << list[i]->m_title << "\n";
    return list[readChoice(1, list.size()) - 1];
}

// Вход системы с одним входом: вручную или из сценария.
engine::FuzzyInput readSingleInput(const db::KnowledgeBase& kb, const engine::Inference& inference) {
    std::cout << "1. Ввести вход\n"
              << "2. Готовый сценарий\n";
    if (readChoice(1, 2) == 2) {
        const db::Scenario* s = readScenario(kb, "single");
        if (s != nullptr)
            return inference.makeInputs(*s)[0];
    }
    return readInput(kb, inference, kb.getSingleInput());
}

// Входы системы с несколькими входами: вручную или из сценария.
std::vector<engine::FuzzyInput> readMultiInput(const db::KnowledgeBase& kb, const engine::Inference& inference) {
    std::cout << "1. Ввести входы\n"
              << "2. Готовый сценарий\n";
    if (readChoice(1, 2) == 2) {
        const db::Scenario* s = readScenario(kb, "multi");
        if (s != nullptr)
            return inference.makeInputs(*s);
    }
    std::vector<engine::FuzzyInput> res;
    for (const auto& name : kb.getMultiInputs())
        res.push_back(readInput(kb, inference, name));
    return res;
}

void printSettings(engine::Implication imp, engine::Aggregation agg, int algorithm) {
    std::cout << "Импликация: " << engine::toString(imp) << ", агрегация: " << engine::toString(agg)
              << ", механизм: алгоритм " << algorithm << "\n";
}

int main(int argc, char* argv[]) {
    std::string path = argc > 1 ? argv[1] : db::db_path;
    auto knowledgeBase = std::make_unique<db::KnowledgeBase>();
    if (!knowledgeBase->load(path)) {
        std::cerr << "Ошибка чтения базы знаний " << path << "\n";
        return 1;
    }
    engine::Inference inference(*knowledgeBase);

    engine::Implication implication = engine::Implication::Mamdani;
    engine::Aggregation aggregation = engine::Aggregation::Max;
    int algorithm = 1;

    while (true) {
        std::cout << CYAN << BOLD << "\n=== Нечёткий логический вывод ===" << RESET << "\n"
                  << "База: " << knowledgeBase->getPath() << ". ";
        printSettings(implication, aggregation, algorithm);
        std::cout << "1. Загрузить базу знаний\n"
                  << "2. Показать переменные и термы\n"
                  << "3. Показать правила\n"
                  << "4. Настройки вывода (импликация, агрегация, механизм)\n"
                  << "5. Вывод для одного входа\n"
                  << "6. Вывод для нескольких входов\n"
                  << "7. Сравнение импликаций\n"
                  << "8. Отчёт для записки (графики и таблицы в plots/)\n"
                  << "9. Самопроверка\n"
                  << "0. Выход\n";

        int choice = readChoice(0, 9);
        switch (choice) {
            case 1: {
                std::cout << GREEN << "Введите путь к файлу (пусто — " << db::db_path << "): " << RESET;
                std::string newPath;
                std::getline(std::cin, newPath);
                if (newPath.empty())
                    newPath = db::db_path;
                db::KnowledgeBase loaded;
                if (loaded.load(newPath)) {
                    *knowledgeBase = loaded;   // inference ссылается на тот же объект
                    std::cout << GREEN << "База знаний загружена" << RESET << "\n";
                } else {
                    std::cout << RED << "Ошибка чтения " << newPath << ", осталась прежняя база" << RESET << "\n";
                }
                break;
            }

            case 2: {
                for (const auto& pair : knowledgeBase->getVariables()) {
                    const auto& var = pair.second;
                    const auto& u = var.m_universe;
                    std::cout << YELLOW << BOLD << pair.first << RESET << " — " << var.m_title
                              << (var.m_unit.empty() ? "" : ", " + var.m_unit) << "\n  универсум: ";
                    if (u.m_points.empty())
                        std::cout << engine::fmtTrim(u.m_min, 3) << ".." << engine::fmtTrim(u.m_max, 3)
                                  << ", шаг " << engine::fmtTrim(u.m_step, 3) << "\n";
                    else
                        std::cout << "точки " << engine::fmtTrim(u.m_points.front(), 3) << " … "
                                  << engine::fmtTrim(u.m_points.back(), 3) << " (" << u.m_points.size() << " шт.)\n";
                    for (const auto& t : var.m_terms)
                        std::cout << "  " << engine::pad(t.m_name, 20) << engine::pad(db::toString(t), 24)
                                  << inference.setToString(pair.first, db::termValues(t, u)) << "\n";
                }
                break;
            }

            case 3: {
                std::cout << YELLOW << BOLD << "ПРАВИЛА СИСТЕМЫ С ОДНИМ ВХОДОМ" << RESET << "\n";
                for (const auto& rule : knowledgeBase->getSingleRules())
                    std::cout << "R" << rule.m_id << ". " << toString(rule) << "\n";
                std::cout << YELLOW << BOLD << "ПРАВИЛА СИСТЕМЫ С НЕСКОЛЬКИМИ ВХОДАМИ" << RESET << "\n";
                for (const auto& rule : knowledgeBase->getMultiRules())
                    std::cout << "R" << rule.m_id << ". " << toString(rule) << "\n";
                break;
            }

            case 4: {
                std::cout << YELLOW << BOLD << "ИМПЛИКАЦИЯ" << RESET << "\n"
                          << "1. Мамдани      min(a, b)\n"
                          << "2. Ларсен       a·b\n"
                          << "3. Лукасевич    min(1, 1 − a + b)\n"
                          << "4. Гёдель       1, если a ≤ b, иначе b\n"
                          << "5. Клини–Дайнс  max(1 − a, b)\n";
                implication = engine::allImplications[readChoice(1, 5) - 1];
                aggregation = engine::defaultAggregation(implication);

                std::cout << YELLOW << BOLD << "АГРЕГАЦИЯ" << RESET << " (по умолчанию для этой импликации — "
                          << engine::toString(aggregation) << ")\n"
                          << "1. max\n"
                          << "2. вероятностная сумма  a + b − ab\n"
                          << "3. ограниченная сумма   min(1, a + b)\n"
                          << "4. min\n";
                aggregation = engine::allAggregations[readChoice(1, 4) - 1];

                std::cout << YELLOW << BOLD << "МЕХАНИЗМ ВЫВОДА (для одного входа)" << RESET << "\n"
                          << "1. Алгоритм 1 — сначала выходы правил, потом агрегация\n"
                          << "2. Алгоритм 2 — сначала агрегация матриц правил, потом композиция\n"
                          << "3. Алгоритм 3 — уровни истинности предпосылок\n";
                algorithm = readChoice(1, 3);
                printSettings(implication, aggregation, algorithm);
                break;
            }

            case 5: {
                engine::FuzzyInput in = readSingleInput(*knowledgeBase, inference);
                engine::InferenceResult r = inference.runSingle(in, implication, aggregation, algorithm);
                inference.printSingle(r, in);
                inference.printSummary(in, implication, aggregation);

                std::cout << "Сохранить график этого вывода?\n"
                          << "1. Да\n"
                          << "2. Нет\n";
                if (readChoice(1, 2) == 1) {
                    std::string file = db::plots_path + "/inference_single_alg" + std::to_string(algorithm) + ".svg";
                    engine::Report report(*knowledgeBase);
                    if (report.saveInferencePlot(r, in, false, file))
                        std::cout << GREEN << "Сохранено: " << file << RESET << "\n";
                    else
                        std::cout << RED << "Не удалось сохранить " << file << RESET << "\n";
                }
                break;
            }

            case 6: {
                std::vector<engine::FuzzyInput> in = readMultiInput(*knowledgeBase, inference);
                engine::InferenceResult r = inference.runMulti(in, implication, aggregation);
                inference.printMulti(r, in);

                std::cout << "Сохранить график этого вывода?\n"
                          << "1. Да\n"
                          << "2. Нет\n";
                if (readChoice(1, 2) == 1) {
                    std::string file = db::plots_path + "/inference_multi.svg";
                    engine::Report report(*knowledgeBase);
                    if (report.saveInferencePlot(r, in[0], true, file))
                        std::cout << GREEN << "Сохранено: " << file << RESET << "\n";
                    else
                        std::cout << RED << "Не удалось сохранить " << file << RESET << "\n";
                }
                break;
            }

            case 7: {
                std::cout << "1. Система с одним входом (все импликации × алгоритмы 1–3)\n"
                          << "2. Система с несколькими входами (все импликации, алгоритм 3)\n";
                bool single = readChoice(1, 2) == 1;
                std::cout << "Агрегация:\n"
                          << "1. По умолчанию (max для конъюнктивных, min для логических)\n"
                          << "2. Текущая из настроек (" << engine::toString(aggregation) << ")\n";
                bool byDefault = readChoice(1, 2) == 1;

                std::cout << CYAN << BOLD << "\n=== Сравнение импликаций: y* ===" << RESET << "\n";
                if (single) {
                    engine::FuzzyInput in = readSingleInput(*knowledgeBase, inference);
                    std::cout << BOLD << engine::pad("Импликация", 16) << engine::pad("Agg", 22)
                              << engine::pad("Алгоритм 1", 14) << engine::pad("Алгоритм 2", 14) << "Алгоритм 3" << RESET << "\n";
                    for (auto imp : engine::allImplications) {
                        engine::Aggregation agg = byDefault ? engine::defaultAggregation(imp) : aggregation;
                        std::cout << engine::pad(engine::toString(imp), 16) << engine::pad(engine::toString(agg), 22);
                        for (int alg = 1; alg <= 3; alg++) {
                            engine::InferenceResult r = inference.runSingle(in, imp, agg, alg);
                            std::cout << engine::pad(r.m_defined ? engine::fmt(r.m_crisp_output, 2) : "не опр.", 14);
                        }
                        std::cout << "\n";
                    }
                } else {
                    std::vector<engine::FuzzyInput> in = readMultiInput(*knowledgeBase, inference);
                    std::cout << BOLD << engine::pad("Импликация", 16) << engine::pad("Agg", 22) << engine::pad("y*", 10)
                              << "Ближайший терм" << RESET << "\n";
                    for (auto imp : engine::allImplications) {
                        engine::Aggregation agg = byDefault ? engine::defaultAggregation(imp) : aggregation;
                        engine::InferenceResult r = inference.runMulti(in, imp, agg);
                        std::cout << engine::pad(engine::toString(imp), 16) << engine::pad(engine::toString(agg), 22)
                                  << engine::pad(r.m_defined ? engine::fmt(r.m_crisp_output, 3) : "—", 10)
                                  << inference.conclusion(r) << "\n";
                    }
                }
                break;
            }

            case 8: {
                engine::Report report(*knowledgeBase);
                bool ok = report.generate(db::plots_path);
                for (const auto& f : report.getFiles())
                    std::cout << "  " << f << "\n";
                if (ok)
                    std::cout << GREEN << "Отчёт сформирован. PNG для записки: ./to_png.sh" << RESET << "\n";
                else
                    std::cout << RED << "Отчёт сформирован не полностью" << RESET << "\n";
                break;
            }

            case 9: {
                engine::runSelfTest(db::lecture_path, *knowledgeBase);
                break;
            }

            case 0:
                return 0;
        }
    }
}
