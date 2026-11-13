#pragma once
#include <string>
#include "db/KnowledgeBase.hpp"

namespace engine {
    // контрольные значения: примеры лекции 4 и воронка (п. 2, п. 3); возвращает true, если всё сошлось
    bool runSelfTest(const std::string& lecturePath, const db::KnowledgeBase& funnel);
}
