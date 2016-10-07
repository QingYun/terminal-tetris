#pragma once
#include "../termbox/termbox.h"
#include "./canvas.h"

class Termbox;
class TermboxCanvas : public Canvas {
private:
  // disable manual construction. Ensure that only Termbox can instantiate it, after initializing termbox.
  TermboxCanvas();
  
public:
  int getWidth() const override;
  int getHeight() const override;
  void clear(uint16_t fg = TB_DEFAULT, uint16_t bg = TB_DEFAULT) override;
  void setCell(int, int, uint32_t, uint16_t fg = TB_DEFAULT, uint16_t bg = TB_DEFAULT) override;
  void present() override;

  friend class Termbox;
};

class Termbox {
public:
  // tb_init() + tb_select_output_mode()
  Termbox(int output_mode = TB_OUTPUT_256);
  ~Termbox();
  
  TermboxCanvas getCanvas() const;
};
