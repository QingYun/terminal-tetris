#pragma once
#include <unordered_map>
#include <functional>
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
  using EventHandler = void(Termbox::*)(const tb_event&);
  TermboxCanvas canvas_;
  bool should_exit_;
  std::function<void(int)> updateWindowWidth_, updateWindowHeight_;
  std::unordered_map<int, EventHandler> event_handlers_;

  void render_(ComponentPointer root_elm) override;
  void exit_() override;
  void handleKey_(const tb_event& ev);
  void handleMouse_(const tb_event& ev);
  void handleResize_(const tb_event& ev);

public:
  template <typename Store>
  Termbox(Store& store, int output_mode = TB_OUTPUT_256) : canvas_{}, should_exit_{false}, 
    updateWindowWidth_{[&store] (int width) { 
      store.template dispatch<ACTION(details::BuiltinAction::UpdateWindowWidth)>(width); 
    }},
    updateWindowHeight_{[&store] (int height) { 
      store.template dispatch<ACTION(details::BuiltinAction::UpdateWindowHeight)>(height); 
    }},
    event_handlers_{
      {TB_EVENT_KEY, &Termbox::handleKey_}, 
      {TB_EVENT_MOUSE, &Termbox::handleMouse_}, 
      {TB_EVENT_RESIZE, &Termbox::handleResize_}
    } {
    int ret = tb_init();
    if (ret) {
      // -2 in VS Code debugger
      logger() << "tb_init() failed with error code " << ret;
    }
    tb_clear();
    tb_select_output_mode(output_mode);
    updateWindowWidth_(tb_width());
    updateWindowHeight_(tb_height());
  }

  ~Termbox();

  Canvas& getCanvas() override;
  void runMainLoop(std::chrono::microseconds frame_duration = std::chrono::microseconds{16667}) override;
};
