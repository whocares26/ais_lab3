#pragma once
#include <string>
#include <vector>

namespace engine {
    enum class Implication {
        Mamdani, Larsen, Lukasiewicz, Godel, KleeneDienes
    };

    enum class Aggregation {
        Max, ProbSum, BoundedSum, Min
    };

    inline const std::vector<Implication> allImplications = {
        Implication::Mamdani, Implication::Larsen, Implication::Lukasiewicz,
        Implication::Godel, Implication::KleeneDienes
    };
    inline const std::vector<Aggregation> allAggregations = {
        Aggregation::Max, Aggregation::ProbSum, Aggregation::BoundedSum, Aggregation::Min
    };

    using FuzzySet = std::vector<double>;               // степени в узлах сетки
    using Matrix = std::vector<std::vector<double>>;    // R[x][y]

    double implicate(Implication imp, double a, double b);
    double aggregate(Aggregation agg, double a, double b);
    double neutral(Aggregation agg);                    // 0 для max и сумм, 1 для min
    bool isLogical(Implication imp);                    // Лукасевич, Гёдель, Клини–Дайнс
    Aggregation defaultAggregation(Implication imp);    // max для конъюнктивных, min для логических
    std::string toString(Implication imp);
    std::string toString(Aggregation agg);

    Matrix relation(const FuzzySet& A, const FuzzySet& B, Implication imp);   // R[x][y] = I(A(x), B(y))
    FuzzySet compose(const FuzzySet& A, const Matrix& R);                     // max-min композиция
    double height(const FuzzySet& A, const FuzzySet& B);                      // max_x min(A, B)
    FuzzySet aggregateSets(const std::vector<FuzzySet>& sets, Aggregation agg, size_t size);
    Matrix aggregateMatrices(const std::vector<Matrix>& list, Aggregation agg, size_t rows, size_t cols);
    FuzzySet applyModifier(const FuzzySet& A, const std::string& modifier);  // очень, более_менее
    bool isNeutral(const FuzzySet& A, Aggregation agg);  // все степени равны нейтральному элементу
    bool centroid(const std::vector<double>& ys, const FuzzySet& B, double& result);  // false — B′ пустое

    // форматирование чисел с запятой
    std::string fmt(double v, int prec = 2);
    std::string fmtTrim(double v, int prec = 2);        // без лишних нулей: 0,5  1  0,33
    int textLength(const std::string& s);               // длина utf-8 строки в символах
    std::string pad(const std::string& s, int width);   // дополнить пробелами справа
    std::string display(const std::string& name);       // «низкая_цель» -> «низкая цель»
}
