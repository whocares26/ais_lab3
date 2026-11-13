#pragma once
#include <string>
#include <vector>

namespace engine {
    // линия с маркерами; NaN в m_y — разрыв линии
    struct Series {
        std::string m_label;
        std::vector<double> m_x;
        std::vector<double> m_y;
        std::string m_color;
        std::string m_dash;             // "" — сплошная, иначе stroke-dasharray
        std::string m_marker;           // circle | square | triangle | diamond | cross | none
        double m_width = 2.0;
        double m_opacity = 1.0;
        int m_marker_every = 1;         // маркер в каждой k-й точке
    };

    // вертикальная отметка (y*)
    struct VLine {
        double m_x;
        std::string m_label;
        std::string m_color;
    };

    // столбик; группы идут в порядке первого появления
    struct Bar {
        std::string m_group;            // подпись группы, '\n' — перенос строки
        std::string m_label;            // подпись столбика под ним (может быть пустой)
        double m_value;
        bool m_defined;                 // false — подписать «не определён»
        std::string m_color;
        std::string m_fill;             // solid | hatch | light
    };

    // элемент легенды, не привязанный к линии
    struct LegendItem {
        std::string m_label;
        std::string m_color;
        std::string m_dash;
        std::string m_marker;
        std::string m_fill;             // "" — линия, иначе образец заливки столбика
        double m_width = 2.0;
    };

    struct Panel {
        std::string m_title;
        std::string m_xlabel;
        std::string m_ylabel;
        double m_xmin = 0, m_xmax = 1, m_ymin = 0, m_ymax = 1;
        std::vector<double> m_xticks;
        std::vector<double> m_yticks;
        std::vector<Series> m_series;
        std::vector<VLine> m_vlines;
        std::vector<Bar> m_bars;
        std::vector<LegendItem> m_legend;   // дополнительно к подписям линий
        std::vector<std::string> m_notes;   // пояснения под легендой
        bool m_bar_values = true;           // числа над столбиками
    };

    class SvgFigure {
    public:
        SvgFigure(int width, int height, const std::string& title, int cols, int rows);
        void setPanel(int col, int row, const Panel& panel);
        std::string render() const;
        bool save(const std::string& path) const;
    private:
        int m_width;
        int m_height;
        std::string m_title;
        int m_cols;
        int m_rows;
        std::vector<Panel> m_panels;
        std::vector<bool> m_used;
    };

    std::vector<double> ticks(double min, double max, double step);
}
