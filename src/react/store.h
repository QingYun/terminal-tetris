#pragma once
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/tuple.hpp>
#include "../utils/immutable-struct.hpp"
#include "./reducer.h"

namespace details {

enum class StoreAction {
  INIT
};

using InitStore = EnumValue<StoreAction, StoreAction::INIT>;

}

#define DECL_STATE_OP(s, d, tuple) BOOST_PP_TUPLE_POP_BACK(tuple)
#define DECL_STATE(fields) \
  IMMUTABLE_STRUCT(StateType, BOOST_PP_SEQ_TRANSFORM(DECL_STATE_OP, _, fields))

#define STATE_INIT_OP(s, d, tuple) \
  BOOST_PP_TUPLE_ELEM(2, tuple)<details::InitStore>::reduce<BOOST_PP_TUPLE_ELEM(0, tuple)>()
#define STATE_INIT(fields) BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(STATE_INIT_OP, _, fields))

#define STATE_REDUCERS_OP(s, _, field) \
  next_state.template update<StateType::Field::BOOST_PP_TUPLE_ELEM(1, field)>( \
    BOOST_PP_TUPLE_ELEM(2, field)<Vs...>::reduce( \
      state_.template get<StateType::Field::BOOST_PP_TUPLE_ELEM(1, field)>(), payload...) \
  );
// generate state updating statements
#define STATE_REDUCERS(fields) \
  BOOST_PP_SEQ_FOR_EACH(STATE_REDUCERS_OP, _, fields)

// fields = ((field_type, field_name, field_reducer)(...)...)
#define DECL_STORE(classname, fields) \
  class classname { \
  public: \
    DECL_STATE(fields); \
    using Listener = std::function<void(const StateType&, const StateType&)>; \
  private: \
    StateType state_; \
    std::vector<Listener> listeners_; \
  public: \
    classname() : state_{STATE_INIT(fields)} {} \
    template <typename... Vs, typename... Ts> \
    void dispatch(const Ts&... payload) { \
      StateType next_state = state_; \
      STATE_REDUCERS(fields) \
      if (next_state != state_) { \
        for (auto& listener : listeners_) { \
          listener(state_, next_state); \
        } \
      } \
      state_ = std::move(next_state); \
    } \
    void addListener(Listener listener, bool immediate_call = true) { \
      if (immediate_call) listener(state_, state_); \
      listeners_.push_back(std::move(listener)); \
    } \
  } 
