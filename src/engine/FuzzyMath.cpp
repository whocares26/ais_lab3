#include "engine/FuzzyMath.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace engine {
        double implicate(Implication imp, double a, double b) {
            switch (imp) {
                case Implication::Mamdani:      return std::min(a, b);
                case Implication::Larsen:       return a * b;
                case Implication::Lukasiewicz:  return std::min(1.0, 1.0 - a + b);
                case Implication::Godel:        return a <= b + 1e-12 ? 1.0 : b;   // допуск на погрешность double
                case Implication::KleeneDienes: return std::max(1.0 - a, b);
            }
            return 0.0;
        }

        double aggregate(Aggregation agg, double a, double b) {
            switch (agg) {
                case Aggregation::Max:        return std::max(a, b);
                case Aggregation::ProbSum:    return a + b - a * b;
                case Aggregation::BoundedSum: return std::min(1.0, a + b);
                case Aggregation::Min:        return std::min(a, b);
            }
            return 0.0;
        }

        double neutral(Aggregation agg) {
            return agg == Aggregation::Min ? 1.0 : 0.0;
        }

        bool isLogical(Implication imp) {
            return imp == Implication::Lukasiewicz || imp == Implication::Godel
                || imp == Implication::KleeneDienes;
        }

        Aggregation defaultAggregation(Implication imp) {
            return isLogical(imp) ? Aggregation::Min : Aggregation::Max;
        }

        std::string toString(Implication imp) {
            switch (imp) {
                case Implication::Mamdani:      return "Мамдани";
                case Implication::Larsen:       return "Ларсен";
                case Implication::Lukasiewicz:  return "Лукасевич";
                case Implication::Godel:        return "Гёдель";
                case Implication::KleeneDienes: return "Клини–Дайнс";
            }
            return "";
        }

        std::string toString(Aggregation agg) {
            switch (agg) {
                case Aggregation::Max:        return "max";
                case Aggregation::ProbSum:    return "вероятностная сумма";
                case Aggregation::BoundedSum: return "ограниченная сумма";
                case Aggregation::Min:        return "min";
            }
            return "";
        }

        Matrix relation(const FuzzySet& A, const FuzzySet& B, Implication imp) {
            Matrix R(A.size(), std::vector<double>(B.size()));
            for (size_t x = 0; x < A.size(); x++)
                for (size_t y = 0; y < B.size(); y++)
                    R[x][y] = implicate(imp, A[x], B[y]);
            return R;
        }

        FuzzySet compose(const FuzzySet& A, const Matrix& R) {
            FuzzySet B(R[0].size(), 0.0);
            for (size_t y = 0; y < B.size(); y++)
                for (size_t x = 0; x < A.size(); x++)
                    B[y] = std::max(B[y], std::min(A[x], R[x][y]));
            return B;
        }

        double height(const FuzzySet& A, const FuzzySet& B) {
            double res = 0.0;
            for (size_t x = 0; x < A.size(); x++)
                res = std::max(res, std::min(A[x], B[x]));
            return res;
        }

        // агрегация начинается с нейтрального элемента, а не с нуля: иначе min всё обнулит
        FuzzySet aggregateSets(const std::vector<FuzzySet>& sets, Aggregation agg, size_t size) {
            FuzzySet res(size, neutral(agg));
            for (const auto& s : sets)
                for (size_t k = 0; k < size; k++)
                    res[k] = aggregate(agg, res[k], s[k]);
            return res;
        }

        Matrix aggregateMatrices(const std::vector<Matrix>& list, Aggregation agg, size_t rows, size_t cols) {
            Matrix res(rows, std::vector<double>(cols, neutral(agg)));
            for (const auto& m : list)
                for (size_t x = 0; x < rows; x++)
                    for (size_t y = 0; y < cols; y++)
                        res[x][y] = aggregate(agg, res[x][y], m[x][y]);
            return res;
        }

        FuzzySet applyModifier(const FuzzySet& A, const std::string& modifier) {
            FuzzySet res = A;
            for (auto& v : res) {
                if (modifier == "очень")
                    v = v * v;
                else if (modifier == "более_менее")
                    v = std::sqrt(v);
            }
            return res;
        }

        bool isNeutral(const FuzzySet& A, Aggregation agg) {
            for (double v : A)
                if (std::fabs(v - neutral(agg)) > 1e-12)
                    return false;
            return true;
        }

        bool centroid(const std::vector<double>& ys, const FuzzySet& B, double& result) {
            double num = 0.0, den = 0.0;
            for (size_t i = 0; i < ys.size(); i++) {
                num += ys[i] * B[i];
                den += B[i];
            }
            if (den < 1e-12)
                return false;     // все нули — центр тяжести не определён
            result = num / den;
            return true;
        }

        std::string fmt(double v, int prec) {
            std::ostringstream out;
            out << std::fixed << std::setprecision(prec) << v;
            std::string s = out.str();
            for (auto& c : s)
                if (c == '.') c = ',';
            if (s[0] == '-' && s.find_first_not_of("-0,") == std::string::npos)
                s.erase(0, 1);   // -0,00 -> 0,00
            return s;
        }

        std::string fmtTrim(double v, int prec) {
            std::string s = fmt(v, prec);
            if (s.find(',') != std::string::npos) {
                s.erase(s.find_last_not_of('0') + 1);
                if (s.back() == ',')
                    s.pop_back();
            }
            return s;
        }

        int textLength(const std::string& s) {
            int n = 0;
            for (unsigned char c : s)
                if ((c & 0xC0) != 0x80)
                    n++;
            return n;
        }

        std::string pad(const std::string& s, int width) {
            int n = textLength(s);
            return n >= width ? s : s + std::string(width - n, ' ');
        }

        std::string display(const std::string& name) {
            std::string res = name;
            std::replace(res.begin(), res.end(), '_', ' ');
            return res;
        }
    }
