#include "../termbox/termbox.h"
#include "./termbox.h"
#include "../utils/logger.h"

TermboxCanvas::TermboxCanvas() {}

int TermboxCanvas::getWidth() const {
  return tb_width();
}

int TermboxCanvas::getHeight() const {
  return tb_height();
}

void TermboxCanvas::clear(uint16_t fg, uint16_t bg) {
  tb_set_clear_attributes(fg, bg);
  tb_clear();
}

void TermboxCanvas::setCell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg) {
  tb_change_cell(x, y, ch, fg, bg);
}

void TermboxCanvas::present() {
  tb_present();
}

Termbox::Termbox(int output_mode) {
  int ret = tb_init(); 
  if (ret) {
    // -2 in VS Code debugger
    logger() << "tb_init() failed with error code " << ret;
  }
  tb_clear();
  tb_select_output_mode(output_mode);
}

Termbox::~Termbox() {
  tb_shutdown();
}

TermboxCanvas Termbox::getCanvas() const {
  return TermboxCanvas{};
}