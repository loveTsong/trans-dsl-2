//
// Created by Darwin Yuan on 2020/6/15.
//

#ifndef TRANS_DSL_2_SWITCHCASEHELPER_H
#define TRANS_DSL_2_SWITCHCASEHELPER_H

#include <trans-dsl/sched/concept/TransactionContext.h>
#include <trans-dsl/sched/action/SchedSwitchCase.h>
#include <trans-dsl/sched/action/ActionPath.h>
#include <trans-dsl/sched/helper/IsSchedAction.h>
#include <cub/base/IsClass.h>
#include <trans-dsl/utils/SeqInt.h>
#include <algorithm>
#include "Pred.h"

TSL_NS_BEGIN

namespace details {

   template<typename T_PRED, typename T_ACTION, typename = void>
   struct GenericActionPathClass;

   template<typename T_PRED, typename T_ACTION>
   struct GenericActionPathClass<T_PRED, T_ACTION, IsSchedAction<T_ACTION>> {
      struct Inner : ActionPath {
         OVERRIDE(shouldExecute(const TransactionInfo& trans) -> bool) {
            auto pred = new (cache) T_PRED;
            return (*pred)(trans);
         }

         OVERRIDE(getAction() -> SchedAction&) {
            auto action = new (cache) T_ACTION;
            return *action;
         }

      private:
         alignas(std::max(alignof(T_PRED), alignof(T_ACTION)))
         char cache[std::max(sizeof(T_PRED), sizeof(T_ACTION))];
      };
   };

   template<PredFunction V_PRED, typename T_ACTION>
   struct GenericActionPathFunc {
      struct Inner : ActionPath {
         OVERRIDE(shouldExecute(const TransactionInfo& trans) -> bool) {
            return V_PRED(trans);
         }

         OVERRIDE(getAction() -> SchedAction&) {
            auto action = new (cache) T_ACTION;
            return *action;
         }

      private:
         alignas(T_ACTION) char cache[sizeof(T_ACTION)];
      };
   };

   template<typename T_PRED, typename T_ACTION>
   auto DeduceActionPath() -> typename GenericActionPathClass<T_PRED, T_ACTION>::Inner;

   template<PredFunction V_PRED, typename T_ACTION>
   auto DeduceActionPath() -> typename GenericActionPathFunc<V_PRED, T_ACTION>::Inner;

   //////////////////////////////////////////////////////////////////////////////////////////
   template<size_t T_SIZE, size_t T_ALIGN, SeqInt T_SEQ, typename = void, typename ... T_PATH>
   struct GenericSwitch;

   template<typename T>
   using IsActionPath = std::enable_if_t<std::is_base_of_v<ActionPath, T>>;

   template<size_t T_SIZE, size_t T_ALIGN, SeqInt T_SEQ, typename T_HEAD, typename ... T_TAIL>
   struct GenericSwitch<T_SIZE, T_ALIGN, T_SEQ, IsActionPath<T_HEAD>, T_HEAD, T_TAIL...> {
      using Path  = T_HEAD;
      using Next =
      typename GenericSwitch<
         std::max(T_SIZE, sizeof(Path)),
         std::max(T_ALIGN, alignof(Path)),
         T_SEQ + 1,
         void,
         T_TAIL...>::Inner;

      struct Inner : Next {
         using Next::cache;

         auto get(SeqInt seq) -> ActionPath* {
            return seq == T_SEQ ? new (cache) Path : Next::get(seq);
         }
      };
   };

   template<size_t T_SIZE, size_t T_ALIGN, SeqInt T_SEQ>
   struct GenericSwitch<T_SIZE, T_ALIGN, T_SEQ> {
      struct Inner  {
         auto get(SeqInt seq) -> ActionPath* {
            return nullptr;
         }
      protected:
         alignas(T_ALIGN) char cache[T_SIZE];
      };
   };

   template<typename ... T_PATHS>
   struct SWITCH__  {
      using Switch = typename GenericSwitch<0, 0, 0, void, T_PATHS...>::Inner;

      static_assert(sizeof...(T_PATHS) >= 2, "should have at least 2 __case, or use __optional instead");

      struct Inner : Switch, SchedSwitchCase {
      private:
         SeqInt i = 0;
         OVERRIDE(getNext() -> ActionPath *) {
            return Switch::get(i++);
         }
      };
   };

   auto IsTrue__(const TransactionInfo&) -> bool { return true; }
}

TSL_NS_END

#define __case(pred, action) decltype(TSL_NS::details::DeduceActionPath<pred, action>())
#define __otherwise(action) __case(TSL_NS::details::IsTrue__, action)
#define __switch(...) TSL_NS::details::SWITCH__<__VA_ARGS__>::Inner

#endif //TRANS_DSL_2_SWITCHCASEHELPER_H