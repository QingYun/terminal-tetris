#include <string>
#include "canvas.h"

Canvas::~Canvas() {}

void Canvas::writeString(int x, int y, std::string str, uint16_t fg, uint16_t bg) {
  for (std::size_t i = 0; i < str.size(); ++i ) {
    setCell(x + i, y, str[i], fg, bg);
  }
}
