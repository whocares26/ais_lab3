#include "engine/SvgPlot.hpp"
#include "engine/FuzzyMath.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <set>

namespace engine {

        static const char* FONT = "DejaVu Sans, Arial, sans-serif";
        static const char* INK = "#0b0b0b";      // основной текст
        static const char* INK2 = "#52514e";     // подписи осей, пояснения
        static const char* GRID = "#e1e0d9";     // светлая сетка
        static const char* AXIS = "#898781";     // тонкие оси
        // кегль: рисунок 900 px вставляется на 16,5 см, 16 px ≈ 8 pt
        static const int FS = 16;      // подписи, легенда, пояснения
        static const int FT = 18;      // заголовок панели
        static const int FF = 20;      // заголовок рисунка

        static std::string esc(const std::string& s) {
            std::string res;
            for (char c : s) {
                if (c == '&') res += "&amp;";
                else if (c == '<') res += "&lt;";
                else if (c == '>') res += "&gt;";
                else res += c;
            }
            return res;
        }

        // координаты в svg — с точкой
        static std::string n(double v) {
            std::ostringstream out;
            out << std::fixed << std::setprecision(1) << v;
            return out.str();
        }

        // ширина текста DejaVu Sans, оценка сверху
        static double tw(const std::string& s, double size) {
            return textLength(s) * size * 0.62;
        }

        // перенос по словам под заданную ширину
        static std::vector<std::string> wrap(const std::string& s, double size, double width) {
            std::vector<std::string> lines;
            std::istringstream words(s);
            std::string word, cur;
            while (words >> word) {
                std::string next = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && tw(next, size) > width) {
                    lines.push_back(cur);
                    cur = word;
                } else {
                    cur = next;
                }
            }
            if (!cur.empty())
                lines.push_back(cur);
            return lines;
        }

        static std::string text(double x, double y, const std::string& s, int size, const std::string& anchor,
                                const std::string& color = INK, const std::string& extra = "") {
            return "<text x=\"" + n(x) + "\" y=\"" + n(y) + "\" font-size=\"" + std::to_string(size)
                 + "\" text-anchor=\"" + anchor + "\" fill=\"" + color + "\"" + extra + ">" + esc(s) + "</text>\n";
        }

        static std::string line(double x1, double y1, double x2, double y2, const std::string& color,
                                double width, const std::string& dash = "") {
            std::string res = "<line x1=\"" + n(x1) + "\" y1=\"" + n(y1) + "\" x2=\"" + n(x2) + "\" y2=\"" + n(y2)
                            + "\" stroke=\"" + color + "\" stroke-width=\"" + n(width) + "\"";
            if (!dash.empty())
                res += " stroke-dasharray=\"" + dash + "\"";
            return res + "/>\n";
        }

        static std::string marker(const std::string& type, double x, double y, const std::string& color, double opacity, double k = 1.0) {
            std::string op = opacity < 1.0 ? " opacity=\"" + n(opacity) + "\"" : "";
            std::string ring = " stroke=\"#ffffff\" stroke-width=\"1\"" + op;   // светлое кольцо поверх соседних линий
            if (type == "circle")
                return "<circle cx=\"" + n(x) + "\" cy=\"" + n(y) + "\" r=\"" + n(4 * k) + "\" fill=\"" + color + "\"" + ring + "/>\n";
            if (type == "square")
                return "<rect x=\"" + n(x - 4 * k) + "\" y=\"" + n(y - 4 * k) + "\" width=\"" + n(8 * k) + "\" height=\"" + n(8 * k) + "\" fill=\"" + color + "\"" + ring + "/>\n";
            if (type == "triangle")
                return "<polygon points=\"" + n(x) + "," + n(y - 5.5 * k) + " " + n(x + 5 * k) + "," + n(y + 4 * k) + " " + n(x - 5 * k) + ","
                     + n(y + 4 * k) + "\" fill=\"" + color + "\"" + ring + "/>\n";
            if (type == "diamond")
                return "<polygon points=\"" + n(x) + "," + n(y - 5.5 * k) + " " + n(x + 5.5 * k) + "," + n(y) + " " + n(x) + ","
                     + n(y + 5.5 * k) + " " + n(x - 5.5 * k) + "," + n(y) + "\" fill=\"" + color + "\"" + ring + "/>\n";
            if (type == "cross")
                return line(x - 4 * k, y - 4 * k, x + 4 * k, y + 4 * k, color, 2 * k) + line(x - 4 * k, y + 4 * k, x + 4 * k, y - 4 * k, color, 2 * k);
            return "";
        }

        static std::string hatchId(const std::string& color) {
            return "hatch" + color.substr(1);
        }

        // заливка столбика: сплошная, штриховка или светлая — различимы и в ч/б
        static std::string barStyle(const std::string& fill, const std::string& color) {
            if (fill == "hatch")
                return "fill=\"url(#" + hatchId(color) + ")\" stroke=\"" + color + "\" stroke-width=\"1.5\"";
            if (fill == "light")
                return "fill=\"" + color + "\" fill-opacity=\"0.25\" stroke=\"" + color + "\" stroke-width=\"1.5\"";
            return "fill=\"" + color + "\"";
        }

        std::vector<double> ticks(double min, double max, double step) {
            std::vector<double> res;
            for (int i = 0; min + i * step <= max + 1e-9; i++)
                res.push_back(min + i * step);
            return res;
        }

        SvgFigure::SvgFigure(int width, int height, const std::string& title, int cols, int rows)
            : m_width(width), m_height(height), m_title(title), m_cols(cols), m_rows(rows),
              m_panels(cols * rows), m_used(cols * rows, false) {}

        void SvgFigure::setPanel(int col, int row, const Panel& panel) {
            m_panels[row * m_cols + col] = panel;
            m_used[row * m_cols + col] = true;
        }

        static void renderPanel(std::ostringstream& o, const Panel& p, double px, double py, double pw, double ph, int id) {
            double left = px + (p.m_ylabel.empty() ? 54 : 84);
            double right = px + pw - 20;
            double band = p.m_vlines.size() * 24;   // полоса над графиком под подписи y*
            double top = py + (p.m_title.empty() ? 16 : 48) + band;

            // легенда: подписи линий + дополнительные элементы, перенос по строкам
            std::vector<LegendItem> items;
            for (const auto& s : p.m_series)
                if (!s.m_label.empty())
                    items.push_back({s.m_label, s.m_color, s.m_dash, s.m_marker, "", s.m_width});
            for (const auto& it : p.m_legend)
                items.push_back(it);
            std::vector<double> itemX;
            std::vector<int> itemRow;
            double cx = left;
            int row = 0;
            for (const auto& it : items) {
                double w = 38 + tw(it.m_label, FS) + 24;
                if (cx > left && cx + w > right) {
                    row++;
                    cx = left;
                }
                itemX.push_back(cx);
                itemRow.push_back(row);
                cx += w;
            }
            int legendRows = items.empty() ? 0 : row + 1;

            bool bars = !p.m_bars.empty();
            std::vector<std::string> groups;
            bool barSub = false;
            int groupLines = 1;
            for (const auto& b : p.m_bars) {
                if (std::find(groups.begin(), groups.end(), b.m_group) == groups.end())
                    groups.push_back(b.m_group);
                if (!b.m_label.empty())
                    barSub = true;
                groupLines = std::max(groupLines, (int)std::count(b.m_group.begin(), b.m_group.end(), '\n') + 1);
            }

            double xaxisH = bars ? (barSub ? 24 : 0) + groupLines * 21 + 8 : 26;
            double xlabelH = p.m_xlabel.empty() ? 0 : 26;
            double legendH = legendRows ? legendRows * 24 + 8 : 0;
            std::vector<std::string> notes;
            for (const auto& note : p.m_notes)
                for (const auto& l : wrap(note, FS, right - left))
                    notes.push_back(l);
            double notesH = notes.size() * 22;
            double bottom = py + ph - 10 - notesH - legendH - xlabelH - xaxisH;

            auto X = [&](double v) { return left + (v - p.m_xmin) / (p.m_xmax - p.m_xmin) * (right - left); };
            auto Y = [&](double v) { return bottom - (v - p.m_ymin) / (p.m_ymax - p.m_ymin) * (bottom - top); };

            if (!p.m_title.empty())
                o << text(px + pw / 2, py + 30, p.m_title, FT, "middle", INK, " font-weight=\"bold\"");

            // сетка и подписи по y
            for (double v : p.m_yticks) {
                o << line(left, Y(v), right, Y(v), GRID, 1);
                o << text(left - 8, Y(v) + 5.5, fmtTrim(v, 2), FS, "end", INK2);
            }

            // сетка и подписи по x; подписи прореживаются, если не помещаются
            if (!bars && !p.m_xticks.empty()) {
                double maxw = 0;
                for (double v : p.m_xticks)
                    maxw = std::max(maxw, tw(fmtTrim(v, 2), FS));
                double spacing = p.m_xticks.size() > 1 ? X(p.m_xticks[1]) - X(p.m_xticks[0]) : 1000;
                int k = 1;
                while (spacing * k < maxw + 8)
                    k++;
                for (size_t i = 0; i < p.m_xticks.size(); i++) {
                    double v = p.m_xticks[i];
                    o << line(X(v), top, X(v), bottom, GRID, 1);
                    if (i % k == 0)
                        o << text(X(v), bottom + 20, fmtTrim(v, 2), FS, "middle", INK2);
                }
            }

            o << line(left, bottom, right, bottom, AXIS, 1);
            o << line(left, top, left, bottom, AXIS, 1);

            // столбики
            if (bars) {
                double slot = (right - left) / groups.size();
                int k = 1;
                for (const auto& g : groups) {
                    int cnt = 0;
                    for (const auto& b : p.m_bars)
                        if (b.m_group == g) cnt++;
                    k = std::max(k, cnt);
                }
                double bw = std::min(44.0, slot * 0.72 / k);
                double gap = 3;
                for (size_t g = 0; g < groups.size(); g++) {
                    std::vector<const Bar*> list;
                    for (const auto& b : p.m_bars)
                        if (b.m_group == groups[g])
                            list.push_back(&b);
                    double total = list.size() * bw + (list.size() - 1) * gap;
                    double x0 = left + slot * (g + 0.5) - total / 2;
                    for (size_t j = 0; j < list.size(); j++) {
                        const Bar& b = *list[j];
                        double bx = x0 + j * (bw + gap);
                        if (b.m_defined) {
                            double y0 = Y(std::max(b.m_value, p.m_ymin));
                            o << "<rect x=\"" << n(bx) << "\" y=\"" << n(y0) << "\" width=\"" << n(bw)
                              << "\" height=\"" << n(bottom - y0) << "\" " << barStyle(b.m_fill, b.m_color) << "/>\n";
                            if (p.m_bar_values && b.m_value > 1e-9)
                                o << text(bx + bw / 2, y0 - 7, fmt(b.m_value, 2), FS, "middle");
                        } else {
                            o << text(bx + bw / 2 + 5.5, bottom - 6, "не определён", FS, "start", INK2,
                                      " transform=\"rotate(-90 " + n(bx + bw / 2 + 5.5) + " " + n(bottom - 6) + ")\"");
                        }
                        if (!b.m_label.empty())
                            o << text(bx + bw / 2, bottom + 20, b.m_label, FS, "middle", INK2);
                    }
                    // подпись группы, возможно в несколько строк
                    std::istringstream lines(groups[g]);
                    std::string part;
                    int ln = 0;
                    while (std::getline(lines, part)) {
                        o << text(left + slot * (g + 0.5), bottom + (barSub ? 24 : 0) + 20 + ln * 21, part, FS, "middle");
                        ln++;
                    }
                }
            }

            // линии с маркерами, обрезка по области построения
            o << "<clipPath id=\"clip" << id << "\"><rect x=\"" << n(left - 7) << "\" y=\"" << n(top - 7)
              << "\" width=\"" << n(right - left + 14) << "\" height=\"" << n(bottom - top + 14) << "\"/></clipPath>\n";
            o << "<g clip-path=\"url(#clip" << id << ")\">\n";
            for (const auto& s : p.m_series) {
                std::string d;
                bool pen = false;
                for (size_t i = 0; i < s.m_x.size(); i++) {
                    if (std::isnan(s.m_y[i])) {
                        pen = false;   // разрыв: y* не определён
                        continue;
                    }
                    d += (pen ? " L" : " M") + n(X(s.m_x[i])) + " " + n(Y(s.m_y[i]));
                    pen = true;
                }
                o << "<path d=\"" << d << "\" fill=\"none\" stroke=\"" << s.m_color << "\" stroke-width=\"" << n(s.m_width)
                  << "\" stroke-linejoin=\"round\" stroke-linecap=\"round\"";
                if (!s.m_dash.empty())
                    o << " stroke-dasharray=\"" << s.m_dash << "\"";
                if (s.m_opacity < 1.0)
                    o << " opacity=\"" << n(s.m_opacity) << "\"";
                o << "/>\n";
                for (size_t i = 0; i < s.m_x.size(); i += s.m_marker_every)
                    if (!std::isnan(s.m_y[i]))
                        o << marker(s.m_marker, X(s.m_x[i]), Y(s.m_y[i]), s.m_color, s.m_opacity,
                                    std::min(1.7, std::max(1.0, s.m_width / 2.5)));   // у широкой линии маркер крупнее
            }
            o << "</g>\n";

            // отметки y*: подписи лесенкой в полосе над графиком, чтобы не налезали друг на друга и на данные
            for (size_t i = 0; i < p.m_vlines.size(); i++) {
                const auto& v = p.m_vlines[i];
                if (v.m_x < p.m_xmin || v.m_x > p.m_xmax)
                    continue;
                double x = X(v.m_x);
                o << line(x, top - band + 4, x, bottom, v.m_color, 1.8, "6,4");
                double ty = top - band + 20 + i * 24;
                double w = tw(v.m_label, FS);
                bool toRight = x + 6 + w <= right;
                double tx = toRight ? x + 6 : x - 6;
                o << "<rect x=\"" << n(toRight ? tx - 2 : tx - w - 2) << "\" y=\"" << n(ty - 16) << "\" width=\""
                  << n(w + 4) << "\" height=\"21\" fill=\"#ffffff\" fill-opacity=\"0.85\"/>\n";
                o << line(toRight ? tx - 2 : tx + 2, ty - 16, toRight ? tx - 2 : tx + 2, ty + 5, v.m_color, 3);
                o << text(tx + (toRight ? 3 : -3), ty, v.m_label, FS, toRight ? "start" : "end");
            }

            // подписи осей
            if (!p.m_xlabel.empty())
                o << text((left + right) / 2, bottom + xaxisH + 19, p.m_xlabel, FS, "middle", INK2);
            if (!p.m_ylabel.empty()) {
                double cy = (top + bottom) / 2;
                o << text(px + 22, cy, p.m_ylabel, FS, "middle", INK2,
                          " transform=\"rotate(-90 " + n(px + 22) + " " + n(cy) + ")\"");
            }

            // легенда
            double ly = bottom + xaxisH + xlabelH + 8 + 17;
            for (size_t i = 0; i < items.size(); i++) {
                const auto& it = items[i];
                double x = itemX[i];
                double y = ly + itemRow[i] * 24;
                if (it.m_fill.empty()) {
                    o << line(x, y - 5.5, x + 30, y - 5.5, it.m_color, std::min(it.m_width, 3.5), it.m_dash);
                    o << marker(it.m_marker, x + 15, y - 5.5, it.m_color, 1.0);
                } else {
                    o << "<rect x=\"" << n(x + 3) << "\" y=\"" << n(y - 15) << "\" width=\"24\" height=\"16\" "
                      << barStyle(it.m_fill, it.m_color) << "/>\n";
                }
                o << text(x + 38, y, it.m_label, FS, "start");
            }

            // пояснения
            double ny = bottom + xaxisH + xlabelH + legendH + 17;
            for (size_t i = 0; i < notes.size(); i++)
                o << text(left, ny + i * 22, notes[i], FS, "start", INK2);
        }

        std::string SvgFigure::render() const {
            std::ostringstream o;
            o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << m_width << "\" height=\"" << m_height
              << "\" viewBox=\"0 0 " << m_width << " " << m_height << "\" font-family=\"" << FONT << "\">\n";
            o << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";

            // штриховки для столбиков
            std::set<std::string> hatches;
            for (const auto& p : m_panels) {
                for (const auto& b : p.m_bars)
                    if (b.m_fill == "hatch") hatches.insert(b.m_color);
                for (const auto& it : p.m_legend)
                    if (it.m_fill == "hatch") hatches.insert(it.m_color);
            }
            o << "<defs>\n";
            for (const auto& c : hatches)
                o << "<pattern id=\"" << hatchId(c) << "\" width=\"6\" height=\"6\" patternUnits=\"userSpaceOnUse\" "
                  << "patternTransform=\"rotate(45)\"><rect width=\"6\" height=\"6\" fill=\"#ffffff\"/>"
                  << "<line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"6\" stroke=\"" << c << "\" stroke-width=\"3\"/></pattern>\n";
            o << "</defs>\n";

            // заголовок фигуры, при необходимости в две строки и больше
            std::vector<std::string> title = wrap(m_title, FF * 1.12, m_width - 40);   // полужирный шире обычного
            double titleH = title.empty() ? 0 : 18 + title.size() * 26;
            for (size_t i = 0; i < title.size(); i++)
                o << text(m_width / 2.0, 30 + i * 26, title[i], FF, "middle", INK, " font-weight=\"bold\"");

            double pw = (double)m_width / m_cols;
            double ph = (m_height - titleH) / m_rows;
            for (int r = 0; r < m_rows; r++)
                for (int c = 0; c < m_cols; c++)
                    if (m_used[r * m_cols + c])
                        renderPanel(o, m_panels[r * m_cols + c], c * pw, titleH + r * ph, pw, ph, r * m_cols + c);
            o << "</svg>\n";
            return o.str();
        }

        bool SvgFigure::save(const std::string& path) const {
            std::ofstream file(path);
            if (!file.is_open())
                return false;
            file << render();
            return true;
        }
    }
