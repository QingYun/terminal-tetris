#pragma once
#include "./provider.h"
#include "../termbox/termbox.h"
#include "./canvas.h"
#include "./component.h"

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

class Termbox : public Provider {
private:
  TermboxCanvas canvas_;
  bool should_exit_;

  void render_(ComponentPointer root_elm) override;
  void exit_() override;
  void handleEvent_(int event_type, const tb_event& ev);

public:
  // tb_init() + tb_select_output_mode()
  Termbox(int output_mode = TB_OUTPUT_256);
  ~Termbox();

  Canvas& getCanvas() override;
  void runMainLoop(std::chrono::microseconds frame_duration = std::chrono::microseconds{16667}) override;
};
