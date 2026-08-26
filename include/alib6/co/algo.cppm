/**
 * @file algo.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 协程调度算法 (wait_until, any, combine_tasks) (alib6.co:algo)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <generator>
#include <tuple>
#include <utility>
#include <cstddef>

export module alib6.co:algo;

import alib6.core;
import :concepts;
import :task;
import :sync;

export namespace alib6::co {

    template<class T, CanUntil Until>
    void wait_until(Task<T>& t, Until& ut) {
        while (!ut.until()) {
            if (!t.should_next()) alib6::panic("Task expired!");
            t.next();
        }
    }

    template<class T, CanUntil... Ts>
    std::size_t any(Task<T>& task, Ts&&... ts) {
        Race race(std::forward<Ts>(ts)...);
        while (true) {
            if (race.until()) break;
            if (task.should_next()) {
                task.next();
            } else {
                alib6::panic("Task expired!");
            }
        }
        return race.winner;
    }

    template<IsTask... Ts>
    auto combine_tasks(Ts&&... itasks) {
        auto nice_forward = []<class T>(T&& t) -> auto {
            return Task(std::forward<T>(t));
        };

        auto combined_gen = [](auto tup) -> std::generator<int> {
            bool failed = true;
            while (true) {
                failed = true;
                std::apply([&failed](auto&... sub_tasks) {
                    auto execute = [&failed](auto& st) {
                        if (st.should_next()) {
                            st.next();
                            failed = false;
                        }
                    };
                    (execute(sub_tasks), ...);
                }, tup);

                if (failed) alib6::panic("All tasks expired!");
                co_yield 0;
            }
        }(std::make_tuple(nice_forward(std::forward<Ts>(itasks))...));

        return Task(std::move(combined_gen));
    }

    template<IsTask L, IsTask... Ts>
    auto combine_tasks(OnErr<L> fallback, Ts&&... itasks) {
        auto nice_forward = []<class T>(T&& t) -> auto {
            return Task(std::forward<T>(t));
        };

        auto combined_gen = [](auto fb_task, auto tup) -> std::generator<int> {
            bool failed = true;
            while (true) {
                failed = true;
                std::apply([&failed](auto&... sub_tasks) {
                    auto execute = [&failed](auto& st) {
                        if (st.should_next()) {
                            st.next();
                            failed = false;
                        }
                    };
                    (execute(sub_tasks), ...);
                }, tup);

                if (failed) fb_task.next();
                co_yield 0;
            }
        }(std::move(fallback.task), std::make_tuple(nice_forward(std::forward<Ts>(itasks))...));

        return Task(std::move(combined_gen));
    }

} // namespace alib6::co
