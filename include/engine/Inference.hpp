#pragma once
#include <string>
#include <vector>
#include <utility>
#include "db/KnowledgeBase.hpp"
#include "engine/FuzzyMath.hpp"

namespace engine {
    // вход одной переменной, приведённый к сетке
    struct FuzzyInput {
        std::string m_var;
        FuzzySet m_set;         // A′ в узлах сетки (для чёткого — синглтон в ближайшем узле)
        bool m_crisp = false;
        double m_value = 0.0;   // чёткое значение x0
        std::string m_label;    // как вход назван в протоколе и на графиках
    };

    struct InferenceResult {
        Implication m_implication;
        Aggregation m_aggregation;
        int m_algorithm;                            // 1, 2, 3
        std::vector<double> m_alpha;                // α_i по всем правилам
        std::vector<std::vector<double>> m_input_mu; // [правило][вход], -1 если вход не в предпосылке
        std::vector<Matrix> m_relations;            // R_i (алгоритмы 1, 2)
        Matrix m_aggregated;                        // R = Agg(R_i) (алгоритм 2)
        std::vector<FuzzySet> m_rule_outputs;       // B′_i (алгоритмы 1, 3)
        FuzzySet m_output;                          // B′
        bool m_defined = false;                     // false — B′ пустое
        double m_crisp_output = 0.0;                // y*
        bool m_snapped = false;                     // чёткий вход перенесён в узел сетки
        double m_snapped_x = 0.0;
    };

    class Inference {
    public:
        Inference(const db::KnowledgeBase& kb) : m_kb(kb) {}
        FuzzyInput makeInput(const db::InputSpec& spec) const;
        std::vector<FuzzyInput> makeInputs(const db::Scenario& s) const;
        InferenceResult runSingle(const FuzzyInput& in, Implication imp, Aggregation agg, int algorithm) const;
        InferenceResult runMulti(const std::vector<FuzzyInput>& in, Implication imp, Aggregation agg) const;
        void printSingle(const InferenceResult& r, const FuzzyInput& in) const;
        void printMulti(const InferenceResult& r, const std::vector<FuzzyInput>& in) const;
        std::string setToString(const std::string& var, const FuzzySet& A) const;  // 30/0,25 + 40/0,75
        void printSummary(const FuzzyInput& in, Implication imp, Aggregation agg) const;  // алгоритмы 1–3 рядом
        std::pair<std::string, double> nearestTerm(double y) const;   // терм выхода с наибольшей μ в точке y
        std::string conclusion(const InferenceResult& r) const;      // словесный итог для y*
    private:
        const db::KnowledgeBase& m_kb;
        void printMatrix(const Matrix& R, const std::vector<double>& xs, const std::vector<double>& ys) const;
        void printOutputs(const InferenceResult& r, const std::vector<db::FuzzyRule>& rules) const;
        void printResult(const InferenceResult& r) const;
    };

    std::string algorithmName(int algorithm);
}
