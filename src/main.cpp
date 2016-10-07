#include <string>
#include <iostream>
#include <type_traits>
#include "./termbox/termbox.h"
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"
#include "./react/component.h"

using namespace details;

class F : public Component {
  DECL_PROPS(
    ((int, number))
  );

public:
  F(Props props) : props_(std::move(props)) {
    logger() << "F constructor";
  }

  void render() {
    logger() << "F render " << PROPS(number);
  }

  void componentWillUpdate(const Props&) {}
};

class E : public EndComponent {
  DECL_PROPS();

public:
  E(Props props) : props_{std::move(props)} {
    logger() << "E constructor";
  }

  void render() {
    for (auto& pc : PROPS(children)) {
      pc->render();
    }
  }

  void componentWillUpdate(const Props&) {}
};

class D : public Component {
  DECL_PROPS(
    ((int, number))
  );
public:
  D(Props props) : props_{std::move(props)} {
    logger() << "D constructor";
  }

  void render() {
    COMPONENT(E, ATTRIBUTES()) {
      if (*trivial_creator) { *trivial_creator = false; return; }
      for (auto it = std::rbegin(PROPS(children)); it != std::rend(PROPS(children)); ++it) {
        next_children->push_front(*it);
      }
    };
  }

  void componentWillUpdate(const Props&) {
  }

};

class C : public Component {
  DECL_PROPS(
    ((int, number))
  );
public:
  C() {
    logger() << "C constructor";
  }
  void render() {
    logger() << "C render " << PROPS(number);
    COMPONENT(D, ATTRIBUTES(((number, PROPS(number) * 2)))) {
      CHILD_COMPONENT(F, "F1", ATTRIBUTES(((number, PROPS(number) % 2 == 0 ? 2 : 1)))) { NO_CHILDREN };
      CHILD_COMPONENT(F, "F2", ATTRIBUTES(((number, PROPS(number))))) { NO_CHILDREN };
    };
  }

  void componentWillUpdate(const Props&) {
  }

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
  Logger::init(Logger::createTerminal());
  C root{};
  Store<State> store;
  store.addListener([&root](const State &, const State &next_state) {
    auto next_props = root.getProps();
    next_props.update<C::Props::Field::number>(
        next_state.get<State::Field::number1>() + next_state.get<State::Field::number2>());
    if (next_props != root.getProps()) {
      root.componentWillUpdate(next_props);
      root.setProps(std::move(next_props));
      root.render();
    }
  });
  store.dispatch<Action::SET_NUMBER2>(1);
  store.dispatch<Action::SET_NUMBER1>(10);
  store.dispatch<Action::INCREASE_NUMBER1>();
  store.dispatch<Action::INCREASE_NUMBER2>();
  store.dispatch<Action::INCREASE_NUMBER>();
  std::cin.get();
}
