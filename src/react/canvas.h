#pragma once
#include <string>

class Canvas {
public:
  virtual int getWidth() const = 0;
  virtual int getHeight() const = 0;
  // NOTE: default values will be overrided by concrete implementations
  virtual void clear(uint16_t foreground = 0, uint16_t background = 0) = 0;
  // set content and style for a single cell.
  // NOTE: default values will be overrided by concrete implementations
  virtual void setCell(int x, int y, uint32_t ch, uint16_t fg = 0, uint16_t bg = 0) = 0;
  // output a string starting from the position given. 
  // NOTE: default values will be overrided by concrete implementations
  virtual void writeString(int x, int y, std::string str, uint16_t fg = 0, uint16_t bg = 0);
  virtual void present() = 0;
  virtual ~Canvas();
};