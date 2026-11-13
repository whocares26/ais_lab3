#pragma once
#include <string>
#include <vector>
#include "db/KnowledgeBase.hpp"
#include "engine/Inference.hpp"
#include "engine/SvgPlot.hpp"

namespace engine {
    class Report {
    public:
        Report(const db::KnowledgeBase& kb) : m_kb(kb), m_inference(kb) {}
        bool generate(const std::string& dir);   // графики 01–10, results.md, results.csv
        bool saveInferencePlot(const InferenceResult& r, const FuzzyInput& in, bool multi, const std::string& path) const;
        const std::vector<std::string>& getFiles() const;
    private:
        const db::KnowledgeBase& m_kb;
        Inference m_inference;
        std::vector<std::string> m_files;
        bool save(const SvgFigure& fig, const std::string& dir, const std::string& name);
        Panel termsPanel(const std::string& var) const;
        Panel outputPanel(const InferenceResult& r, const std::vector<db::FuzzyRule>& rules, const std::string& title) const;
        Panel sweepPanel(const std::string& var, const std::vector<Implication>& list, const std::vector<std::vector<double>>& values,
                         const std::vector<double>& xs, const std::string& title) const;
        bool writeTables(const std::string& dir, const db::Scenario& single, const db::Scenario& multi);
    };
}
