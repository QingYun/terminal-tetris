#include "./provider.h"

void Provider::setRootElm_(std::unique_ptr<ComponentBase> root_elm) {
  root_elm_ = std::move(root_elm);
}

std::unique_ptr<ComponentBase>& Provider::getRootElm_() {
  return root_elm_;
}
