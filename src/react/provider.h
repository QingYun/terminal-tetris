#pragma once
#include "./canvas.h"

class Provider {
public:
  virtual Canvas& getCanvas() = 0;
};