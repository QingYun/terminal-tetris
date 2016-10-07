#include <string>
#include <iostream>
#include <type_traits>
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"
#include "./react/component.h"
#include "./react/termbox.h"

#include <cstring>

using namespace details;

class F : public EndComponent<F> {
  DECL_PROPS(
    ((int, number))
    ((int, row))
  );

public:
  F(Props props) : props_(std::move(props)) {
    logger() << "F constructor";
  }
  
  void present(Canvas& canvas) {
    canvas.writeString(4, PROPS(row), "Component F " + std::to_string(PROPS(number)), TB_RED, TB_WHITE);
  }

  void componentWillUpdate(const Props&) {}
};

class E : public EndComponent<E> {
  DECL_PROPS();

public:
  E(Props props) : props_{std::move(props)} {
    logger() << "E constructor";
  }

  void present(Canvas& canvas) {
    canvas.writeString(0, 0, "Component E");
  }

  void componentWillUpdate(const Props&) {}
};

class D : public Component {
  DECL_PROPS(
    ((int, number))
  );

  void render_() {
    COMPONENT(E, ATTRIBUTES()) {
      if (*trivial_creator) { *trivial_creator = false; return; }
      for (auto it = std::rbegin(PROPS(children)); it != std::rend(PROPS(children)); ++it) {
        next_children->push_front(*it);
      }
    };
  }

public:
  D(Props props) : props_{std::move(props)} {
    logger() << "D constructor";
  }

  void componentWillUpdate(const Props&) {}
};

class C : public Component {
  DECL_PROPS(
    ((int, number))
  );

  void render_() {
    logger() << "C render " << PROPS(number);
    COMPONENT(D, ATTRIBUTES(((number, PROPS(number) * 2)))) {
      CHILD_COMPONENT(F, "F1", ATTRIBUTES(
        ((number, PROPS(number) % 2 == 0 ? 2 : 1))
        ((row, 1))
      )) { NO_CHILDREN };
      CHILD_COMPONENT(F, "F2", ATTRIBUTES(
        ((number, PROPS(number)))
        ((row, 2))
      )) { NO_CHILDREN };
    };
  }

public:
  C() {
    logger() << "C constructor";
  }

  void componentWillUpdate(const Props&) {}
};

IMMUTABLE_STRUCT(State,
  ((int, number1))
  ((int, number2))
);

enum class Action {
  INIT,
  SET_NUMBER1,
  INCREASE_NUMBER1,
  SET_NUMBER2,
  INCREASE_NUMBER2,
  INCREASE_NUMBER
};

template <Action>
class number1Reducer {
public:
  static int reduce(int state, ...) { return state; }
};

template <>
class number1Reducer<Action::INIT> {
public:
  static int reduce() { return 0; }
};

template <>
class number1Reducer<Action::SET_NUMBER1> {
public:
  static int reduce(int, int payload) {
    return payload;
  }
};

template <>
class number1Reducer<Action::INCREASE_NUMBER1> {
public:
  static int reduce(int state) {
    return state + 1;
  }
};

template <>
class number1Reducer<Action::INCREASE_NUMBER> : public number1Reducer<Action::INCREASE_NUMBER1> {};

template <Action>
class number2Reducer {
public:
  static int reduce(int state, ...) { return state; }
};

template <>
class number2Reducer<Action::INIT> {
public:
  static int reduce() { return 0; }
};

template <>
class number2Reducer<Action::SET_NUMBER2> {
public:
  static int reduce(int, int payload) {
    return payload;
  }
};

template <>
class number2Reducer<Action::INCREASE_NUMBER2> {
public:
  static int reduce(int state) {
    return state + 1;
  }
};

template <>
class number2Reducer<Action::INCREASE_NUMBER> : public number2Reducer<Action::INCREASE_NUMBER2> {};

template <typename StateT>
class Store {
public:
  using Listener = std::function<void(const StateT&, const StateT&)>;

private:
  StateT state_;
  std::vector<Listener> listeners_;
public:
  Store() : state_{number1Reducer<Action::INIT>::reduce(), number2Reducer<Action::INIT>::reduce()} {}

  template <Action A, typename... Ts>
  void dispatch(const Ts&... payload) {
    StateT next_state = state_;
    next_state.template update<StateT::Field::number1>(
      number1Reducer<A>::reduce(state_.template get<StateT::Field::number1>(), payload...));
    next_state.template update<StateT::Field::number2>(
      number2Reducer<A>::reduce(state_.template get<StateT::Field::number2>(), payload...));

    if (next_state != state_) {
      for (auto& listener : listeners_) {
        listener(state_, next_state);
      }
    }
    state_ = std::move(next_state);
  }

  void addListener(Listener listener) {
    listener(state_, state_);
    listeners_.push_back(std::move(listener));
  }
};

int main() {
  Termbox tb{TB_OUTPUT_NORMAL};
  Logger::init(Logger::createTerminal());
  C root{};
  Store<State> store;
  store.addListener([&root](const State &, const State &next_state) {
    auto next_props = root.getProps();
    next_props.update<C::Props::Field::number>(
        next_state.get<State::Field::number1>() + next_state.get<State::Field::number2>());
    if (next_props != root.getProps()) {
      ::details::renderComponent<C>(&root, std::move(next_props));
    }
  });
  auto canvas = tb.getCanvas();
  store.dispatch<Action::SET_NUMBER2>(1);
  root.present(canvas, false);
  canvas.present();
  store.dispatch<Action::SET_NUMBER1>(10);
  root.present(canvas, false);
  canvas.present();
  store.dispatch<Action::INCREASE_NUMBER1>();
  root.present(canvas, false);
  canvas.present();
  store.dispatch<Action::INCREASE_NUMBER2>();
  root.present(canvas, false);
  canvas.present();
  store.dispatch<Action::INCREASE_NUMBER>();
  root.present(canvas, false);
  canvas.present();
  
	struct tb_event ev;
	while (tb_poll_event(&ev)) {
      switch (ev.type) {
      case TB_EVENT_KEY:
        switch (ev.key) {
        case TB_KEY_ESC:
          goto done;
          break;
        }
        break;
      case TB_EVENT_RESIZE:
        break;
      }
    }
  done:
  ;
}
