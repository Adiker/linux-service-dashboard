#pragma once
#include "CenterPage.h"
class DockerPage : public CenterPage {
    Q_OBJECT
  public:
    explicit DockerPage(SystemCenter* center, QWidget* parent = nullptr);
};
