#include <string>
#include <iostream>
#include <type_traits>
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"
#include "./react/component.h"
#include "./react/termbox.h"

#include <cstring>


CREATE_END_COMPONENT_CLASS(F) {
  DECL_PROPS(
    ((int, number))
    ((int, row))
  );

public:
  END_COMPONENT_WILL_MOUNT(F) {
    logger() << "F constructor";
  }

  void present(Canvas& canvas) {
    canvas.writeString(4, PROPS(row), "Component F " + std::to_string(PROPS(number)), TB_RED, TB_WHITE);
  }

  void componentWillUpdate(const Props&) {}
};

CREATE_END_COMPONENT_CLASS(E) {
  DECL_PROPS();

public:
  END_COMPONENT_WILL_MOUNT(E) {
    logger() << "E constructor";
  }

  void present(Canvas& canvas) {
    canvas.writeString(0, 0, "Component E");
  }

  void componentWillUpdate(const Props&) {}
};

CREATE_COMPONENT_CLASS(D) {
  DECL_PROPS(
    ((int, number))
  );

  void render_() {
    RENDER_COMPONENT(E, ATTRIBUTES()) {
      RENDER_COMPONENT(PROPS(children));
    };
  }

public:
  COMPONENT_WILL_MOUNT(D) {
    logger() << "D constructor";
  }

  void componentWillUpdate(const Props&) {}
};

CREATE_COMPONENT_CLASS(C) {
  DECL_PROPS(
    ((int, number))
  );

  void render_() {
    logger() << "C render " << PROPS(number);
    RENDER_COMPONENT(D, ATTRIBUTES(((number, PROPS(number) * 2)))) {
      RENDER_COMPONENT(F, "F1", ATTRIBUTES(
        ((number, PROPS(number) % 2 == 0 ? 2 : 1))
        ((row, 1))
      )) { NO_CHILDREN };
      RENDER_COMPONENT(F, "F2", ATTRIBUTES(
        ((number, PROPS(number)))
        ((row, 2))
      )) { NO_CHILDREN };
    };
  }

public:
  COMPONENT_WILL_MOUNT(C) {
    logger() << "C constructor";
  }

  void componentWillUpdate(const Props&) {}
};

template <typename T, T V> class EnumValue {};

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
  INCREASE_NUMBER,
  SET
};

enum class Target {
  NUMBER1,
  NUMBER2
};

template <typename...>
class number1Reducer {
public:
  static int reduce(int state, ...) { return state; }
};

template <>
class number1Reducer<EnumValue<decltype(Action::INIT), Action::INIT>> {
public:
  static int reduce() { return 0; }
};

template <>
class number1Reducer<EnumValue<decltype(Action::SET), Action::SET>, EnumValue<Target, Target::NUMBER1>> {
public:
  static int reduce(int, int payload) {
    return payload;
  }
};

template <>
class number1Reducer<EnumValue<decltype(Action::INCREASE_NUMBER1), Action::INCREASE_NUMBER1>> {
public:
  static int reduce(int state) {
    return state + 1;
  }
};

template <>
class number1Reducer<EnumValue<decltype(Action::INCREASE_NUMBER), Action::INCREASE_NUMBER>> :
  public number1Reducer<EnumValue<decltype(Action::INCREASE_NUMBER1), Action::INCREASE_NUMBER1>> {};

template <typename...>
class number2Reducer {
public:
  static int reduce(int state, ...) { return state; }
};

template <>
class number2Reducer<EnumValue<Action, Action::INIT>> {
public:
  static int reduce() { return 0; }
};

template <>
class number2Reducer<EnumValue<Action, Action::SET_NUMBER2>> {
public:
  static int reduce(int, int payload) {
    return payload;
  }
};

template <>
class number2Reducer<EnumValue<Action, Action::INCREASE_NUMBER2>> {
public:
  static int reduce(int state) {
    return state + 1;
  }
};

template <>
class number2Reducer<EnumValue<Action, Action::INCREASE_NUMBER>> : public number2Reducer<EnumValue<Action, Action::INCREASE_NUMBER2>> {};

template <typename StateT>
class Store {
public:
  using Listener = std::function<void(const StateT&, const StateT&)>;
  using StateType = StateT;

private:
  StateT state_;
  std::vector<Listener> listeners_;
public:
  Store() : state_{number1Reducer<EnumValue<Action, Action::INIT>>::reduce(), number2Reducer<EnumValue<Action, Action::INIT>>::reduce()} {}

  template <typename... Vs, typename... Ts>
  void dispatch(const Ts&... payload) {
    StateT next_state = state_;
    next_state.template update<StateT::Field::number1>(
      number1Reducer<Vs...>::reduce(state_.template get<StateT::Field::number1>(), payload...));
    next_state.template update<StateT::Field::number2>(
      number2Reducer<Vs...>::reduce(state_.template get<StateT::Field::number2>(), payload...));

    if (next_state != state_) {
      for (auto& listener : listeners_) {
        listener(state_, next_state);
      }
    }
    state_ = std::move(next_state);
  }

  void addListener(Listener listener, bool immediate_call = true) {
    if (immediate_call) listener(state_, state_);
    listeners_.push_back(std::move(listener));
  }
};

int main() {
  Logger::init(Logger::createTerminal());
  Termbox tb{TB_OUTPUT_NORMAL};
  Store<State> store;

  std::function<void(const State&, C<Store<State>>&)> updater =
  [] (const State& state, C<Store<State>>& root_elm) {
      auto next_props = root_elm.getProps();
      next_props.update<C<Store<State>>::Props::Field::number>(
        state.get<State::Field::number1>() + state.get<State::Field::number2>()
      );
      if (next_props != root_elm.getProps()) {
        logger() << "rendering root";
        details::renderComponent<C<Store<State>>>(root_elm, std::move(next_props));
      }
    };

  tb.render<C>(store, updater,
    [] (const State& state) {
      return state.get<State::Field::number1>() + state.get<State::Field::number2>() > 99;
    });

  store.dispatch<EnumValue<decltype(Action::SET_NUMBER2), Action::SET_NUMBER2>>(1);
  store.dispatch<EnumValue<Action, Action::SET>, EnumValue<Target, Target::NUMBER1>>(10);
  store.dispatch<EnumValue<Action, Action::INCREASE_NUMBER1>>();
  store.dispatch<EnumValue<Action, Action::INCREASE_NUMBER2>>();
  store.dispatch<EnumValue<Action, Action::INCREASE_NUMBER>>();

  tb.runMainLoop();
}
