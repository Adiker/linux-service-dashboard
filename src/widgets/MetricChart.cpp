#include "MetricChart.h"

#include <QPainter>
#include <QPainterPath>
void MetricChart::addSample(double cpu, double memory) {
    m_samples.append({qBound(0.0, cpu, 100.0), qBound(0.0, memory, 100.0)});
    while (m_samples.size() > 450)
        m_samples.removeFirst();
    update();
}
void MetricChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRectF(8, 0, width() - 16, 36), Qt::AlignVCenter | Qt::TextWordWrap,
               tr("CPU — solid · Memory — dashed · up to 15 minutes of visible sampling"));
    const QRectF area(34, 48, width() - 45, height() - 63);
    p.setPen(palette().color(QPalette::Mid));
    p.drawRect(area);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(0, 58, "100%");
    p.drawText(10, height() - 10, "0%");
    if (m_samples.size() < 2) {
        p.drawText(area, Qt::AlignCenter, tr("Collecting samples…"));
        return;
    }
    for (int series = 0; series < 2; ++series) {
        QPainterPath path;
        for (int i = 0; i < m_samples.size(); ++i) {
            const auto sample = m_samples[i];
            const double value = series == 0 ? sample.x() : sample.y();
            const QPointF point(area.left() + area.width() * i / qMax(1, int(m_samples.size()) - 1),
                                area.bottom() - area.height() * value / 100.0);
            if (!i)
                path.moveTo(point);
            else
                path.lineTo(point);
        }
        QPen pen(palette().color(QPalette::Highlight), series == 0 ? 2 : 1.5, series == 0 ? Qt::SolidLine : Qt::DashLine);
        if (series)
            pen.setColor(palette().color(QPalette::WindowText));
        p.setPen(pen);
        p.drawPath(path);
    }
}
