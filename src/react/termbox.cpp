#include <memory>
#include <chrono>
#include "../termbox/termbox.h"
#include "../utils/logger.h"
#include "./termbox.h"
#include "./component.h"
#include "./store.h"
#include "./action.h"

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

Termbox::~Termbox() {
  tb_shutdown();
}

Canvas& Termbox::getCanvas() {
  return canvas_;
}

void Termbox::render_(ComponentPointer root_elm) {
  setRootElm_(std::move(root_elm));
}

void Termbox::exit_() {
  logger() << "exit";
  should_exit_ = true;
}

void Termbox::handleKey_(const tb_event& evt) {
  getRootElm_()->onKeyPress(evt);
}

void Termbox::handleMouse_(const tb_event&) {
}

void Termbox::handleResize_(const tb_event& evt) {
  updateWindowWidth_(evt.w);
  updateWindowHeight_(evt.h);
}

void Termbox::runMainLoop(std::chrono::microseconds frame_duration) {
  using namespace std::chrono;
  using us = microseconds;
  using ms = milliseconds;
  tb_event ev;
  auto& canvas = getCanvas();
  while (!should_exit_) {
    auto start_time = high_resolution_clock::now();
    canvas.clear();
    getRootElm_()->present(canvas, true);
    canvas.present();
    do {
      us us_elapsed = duration_cast<us>(high_resolution_clock::now() - start_time);
      if (us_elapsed >= frame_duration) break;

      ms ms_to_wait = duration_cast<ms>(frame_duration - us_elapsed);
      int event_type = tb_peek_event(&ev, ms_to_wait.count());
      if (event_type > 0) {
        (this->*event_handlers_[event_type])(ev);
      } else if (event_type == -1) {
        logger() << "An error occured in tb_peek_event";
      }
    } while (true);
  }
}
