#include <string>
#include <iostream>
#include <type_traits>
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"
#include "./react/component.h"
#include "./react/termbox.h"
#include "./react/store.h"
#include "./react/reducer.h"
#include "./react/action.h"

#include <cstring>

enum class Action {
  INIT,
  SET_NUMBER1,
  INCREASE_NUMBER1,
  DECREASE_NUMBER1,
  SET_NUMBER2,
  INCREASE_NUMBER2,
  INCREASE_NUMBER,
  SET
};

enum class Target {
  NUMBER1,
  NUMBER2
};

CREATE_END_COMPONENT_CLASS(F) {
  DECL_PROPS(
    (int, number)
    (int, row)
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
    (int, number)
  );

  void render_() {
    RENDER_COMPONENT(E, ATTRIBUTES()) {
      RENDER_COMPONENT(PROPS(children));
    };
  }
  
  void onKeyPress(const Event& evt) {
    if (evt.key == TB_KEY_ARROW_LEFT) {
      DISPATCH(Action::DECREASE_NUMBER1)();
    }
    if (evt.key == TB_KEY_ARROW_RIGHT) {
      DISPATCH(Action::INCREASE_NUMBER1)();
    }
  }

public:
  COMPONENT_WILL_MOUNT(D) {
    logger() << "D constructor";
  }

  void componentWillUpdate(const Props&) {}
};

CREATE_COMPONENT_CLASS(C) {
  DECL_PROPS(
    (int, number)
  );

  void render_() override {
    logger() << "C render " << PROPS(number);
    RENDER_COMPONENT(D, ATTRIBUTES((number, PROPS(number) * 2))) {
      RENDER_COMPONENT(F, "F1", ATTRIBUTES(
        (number, PROPS(number) % 2 == 0 ? 2 : 1)
        (row, 1)
      )) { NO_CHILDREN };
      RENDER_COMPONENT(F, "F2", ATTRIBUTES(
        (number, PROPS(number))
        (row, 2)
      )) { NO_CHILDREN };
    };
  }
  
  void onStoreUpdate() override {
    logger() << "store updated";
    // auto next_props = getProps();
    // next_props.update<Props::Field::number>(
    //   state.get<State::Field::number1>() + state.get<State::Field::number2>()
    // );
    // if (next_props != root_elm.getProps()) {
    //   logger() << "rendering root";
    //   details::renderComponent<C<Store>>(root_elm, std::move(next_props));
    // }
  }

public:
  COMPONENT_WILL_MOUNT(C) {
    logger() << "C constructor";
  }

  void componentWillUpdate(const Props&) {}
};

REDUCER(number1Reducer, (Action::SET)(Target::NUMBER1), (int, int payload) {
  return payload;
});

PASS_REDUCER(number1Reducer, (int state, ...) { return state; });
REDUCER(number1Reducer, (Action::INCREASE_NUMBER1), (int state) {
  return state + 1;
});

REDUCER(number1Reducer, (Action::DECREASE_NUMBER1), (int state) {
  return state - 1;
});

REDUCER(number1Reducer, (Action::INCREASE_NUMBER), (int state) {
  return state + 1;
});
INIT_REDUCER(number1Reducer, () { return 0; });

INIT_REDUCER(number2Reducer);

REDUCER(number2Reducer, (Action::SET_NUMBER2), (int, int payload) {
  return payload;
});

PASS_REDUCER(number2Reducer);

REDUCER(number2Reducer, (Action::INCREASE_NUMBER2), (int state) {
  return state + 1;
});

REDUCER(number2Reducer, (Action::INCREASE_NUMBER), (int state) {
  return state + 1;
});

DECL_STORE(Store,
  (int, number1, number1Reducer)
  (int, number2, number2Reducer)
);

int main() {
  Logger::init(Logger::createTerminal());
  Termbox tb{TB_OUTPUT_NORMAL};
  Store store;
  using State = Store::StateType;

  std::function<void(const State&, C<Store>&)> updater =
  [] (const State& state, C<Store>& root_elm) {
      auto next_props = root_elm.getProps();
      next_props.update<C<Store>::Props::Field::number>(
        state.get<State::Field::number1>() + state.get<State::Field::number2>()
      );
      if (next_props != root_elm.getProps()) {
        logger() << "rendering root";
        details::renderComponent<C<Store>>(root_elm, std::move(next_props));
      }
    };

  tb.render<C>(store, updater,
    EXIT_COND { return STORE_FIELD(number1) + STORE_FIELD(number2) > 99; },
    EXIT_COND { return STORE_FIELD(number1) < 0; }
  );

  store.dispatch<ACTION(Action::SET_NUMBER2)>(1);
  store.dispatch<ACTION(Action::SET, Target::NUMBER1)>(10);
  store.dispatch<ACTION(Action::INCREASE_NUMBER1)>();
  store.dispatch<ACTION(Action::INCREASE_NUMBER2)>();
  store.dispatch<ACTION(Action::INCREASE_NUMBER)>();

  tb.runMainLoop();
}
