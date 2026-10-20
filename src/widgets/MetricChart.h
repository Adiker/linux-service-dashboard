#pragma once
#include <QPointF>
#include <QVector>
#include <QWidget>
class MetricChart : public QWidget {
    Q_OBJECT
  public:
    explicit MetricChart(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(130);
        setMaximumHeight(180);
    }
    void addSample(double cpu, double memory);

  protected:
    void paintEvent(QPaintEvent*) override;

  private:
    QVector<QPointF> m_samples;
};
