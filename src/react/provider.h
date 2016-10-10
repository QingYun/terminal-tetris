#pragma once
#include <memory>
#include <chrono>
#include "./canvas.h"
#include "./component.h"
#include "../utils/logger.h"

namespace details {

template <typename... Predicates>
class ShouldExit;

template <typename Predicate, typename... RestPredicates>
class ShouldExit<Predicate, RestPredicates...> : public ShouldExit<RestPredicates...> {
private:
  using Next = ShouldExit<RestPredicates...>;
  Predicate predicate_;

public:
  ShouldExit(Predicate&& predicate, RestPredicates&&... rest_predicates)
  : Next{std::forward<RestPredicates>(rest_predicates)...}, predicate_{std::forward<Predicate>(predicate)} {}

  template <typename StateT>
  bool shouldExit(const StateT& state) const {
    return predicate_(state) || Next::shouldExit(state);
  }
};

template <>
class ShouldExit<> {
public:
  template <typename StateT>
  bool shouldExit(const StateT&) const {
    return false;
  }
};

}

class Provider {
private:
  std::unique_ptr<ComponentBase> root_elm_;
  virtual void render_(std::unique_ptr<ComponentBase> root_elm) = 0;
  virtual void exit_() = 0;

protected:
  void setRootElm_(std::unique_ptr<ComponentBase> root_elm);
  std::unique_ptr<ComponentBase>& getRootElm_();

public:
  virtual Canvas& getCanvas() = 0;
  virtual void runMainLoop(std::chrono::microseconds frame_duration = std::chrono::microseconds{16667}) = 0;

  template <template <typename> typename C, typename StoreT, typename... ExitPredicates>
  void render(StoreT& store, std::function<void(const typename StoreT::StateType&, C<StoreT>&)> component_updater, ExitPredicates&&... exit_predicates) {
    using CT = C<StoreT>;
    using State = typename StoreT::StateType;

    // details::ShouldExit<ExitPredicates...> should_exit{std::forward<ExitPredicates>(exit_predicates)...};
    store.addListener([this, should_exit{details::ShouldExit<ExitPredicates...>{std::forward<ExitPredicates>(exit_predicates)...}}] (const State&, const State& next_state) {
      if (should_exit.shouldExit(next_state)) {
        exit_();
      }
    });

    store.addListener([this, component_updater{std::move(component_updater)}] (const State&, const State& next_state) {
      component_updater(next_state, *dynamic_cast<C<StoreT>*>(getRootElm_().get()));
    }, false);

    render_(details::createComponent<CT>(typename CT::Props{}, store));
  }
};
