#pragma once
#include "CenterPage.h"
class OverviewPage : public CenterPage {
  public:
    explicit OverviewPage(SystemCenter* center, QWidget* parent = nullptr) : CenterPage("overview", center, parent) {}
};
