#include <string>
#include <iostream>
#include <type_traits>
#include "./termbox/termbox.h"
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"
#include "./react/component.h"

using namespace details;

class F : public Component {
public:
  IMMUTABLE_STRUCT(Props,
    ((int, number))
    ((Children, children))
  );

private:
  Props props_;

public:
  F(Props props) : props_(std::move(props)) {
    logger() << "F constructor";
  }

  void render() {
    logger() << "F render " << props_.get<Props::Field::number>();
  }

  void componentWillUpdate(const Props&) {}
  Props& getProps() { return props_; }
  void setProps(Props&& next_props) { props_ = std::move(next_props); }
};

class E : public Component {
public:
  IMMUTABLE_STRUCT(Props,
    ((Children, children))
  );

private:
  Props props_;

public:
  E(Props props) : props_{std::move(props)} {
    logger() << "E constructor";
  }

  void render() {
    for (auto& pc : props_.get<Props::Field::children>()) {
      pc->render();
    }
  }

  void componentWillUpdate(const Props&) {}
  Props& getProps() { return props_; }
  void setProps(Props&& next_props) { props_ = std::move(next_props); }
};

class D : public Component {
public:
  IMMUTABLE_STRUCT(Props,
    ((int, number))
    ((Children, children))
  );

private:
  Props props_;

public:
  D(Props props) : props_{std::move(props)} {
    logger() << "D constructor";
  }

  void render() {
    ChildrenCreator __E_CHILDREN_CREATOR;
    details::ComponentRenderer<E> __RENDER_COMPONENT_E{this, __E_CHILDREN_CREATOR, [] (auto props) { return std::move(props); } };
    __E_CHILDREN_CREATOR = [this] (bool* trivial_creator, std::deque<std::shared_ptr<ComponentHolder>>* next_children) {
      if (*trivial_creator) { *trivial_creator = false; return; }
      for (auto it = std::rbegin(props_.get<Props::Field::children>()); it != std::rend(props_.get<Props::Field::children>()); ++it) {
        next_children->push_front(*it);
      }
    };
  }

  void componentWillUpdate(const Props&) {
  }

  Props& getProps() { return props_; }
  void setProps(Props&& next_props) { props_ = std::move(next_props); }
};

class C : public Component {
public:
  IMMUTABLE_STRUCT(Props,
    ((int, number))
    ((Children, children))
  );

private:
  Props props_;

public:
  C() {
    logger() << "C constructor";
  }
  void render() {
    logger() << "C render " << props_.get<Props::Field::number>();
    ChildrenCreator __D_CHILDREN_CREATOR;
    details::ComponentRenderer<D> __RENDER_COMPONENT_D{this, __D_CHILDREN_CREATOR, [this] (D::Props props) {
        props.update<D::Props::Field::number>(getProps().get<Props::Field::number>() * 2);
        return std::move(props);
    }};
    __D_CHILDREN_CREATOR = [this] (bool* trivial_creator, Children* next_children) {
      if (*trivial_creator) { *trivial_creator = false; return; }
      ChildrenCreator __F_CHILDREN_CREATOR;
      details::ChildRenderer<F> __ADD_F_AS_CHILDREN{"F1", *next_children, __F_CHILDREN_CREATOR, [this] (F::Props props) {
        props.update<F::Props::Field::number>(getProps().get<Props::Field::number>() % 2 == 0 ? 2 : 1);
        return std::move(props);
      }};
      __F_CHILDREN_CREATOR = [] (bool*, Children*) {};

      if (*trivial_creator) { *trivial_creator = false; return; }
      ChildrenCreator __F_CHILDREN_CREATOR2;
      details::ChildRenderer<F> __ADD_F_AS_CHILDREN2{"F2", *next_children, __F_CHILDREN_CREATOR2, [this] (F::Props props) {
        props.update<F::Props::Field::number>(getProps().get<Props::Field::number>());
        return std::move(props);
      }};
      __F_CHILDREN_CREATOR2 = [] (bool*, Children*) {};
    };
  }
  void componentWillUpdate(const Props&) {
  }

  Props& getProps() { return props_; }
  void setProps(Props&& next_props) { props_ = std::move(next_props); }
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
