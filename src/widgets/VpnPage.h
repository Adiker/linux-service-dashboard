#pragma once
#include "CenterPage.h"
class VpnPage : public CenterPage {
    Q_OBJECT
  public:
    explicit VpnPage(SystemCenter* center, QWidget* parent = nullptr) : CenterPage("vpn", center, parent) {}
};
