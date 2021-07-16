#ifndef FUNCTION_RANGE_H
#define FUNCTION_RANGE_H

#include "matchit.h"
#include "core.h"
#include "solve.h"

namespace mathiu
{
    namespace impl
    {
        // implement limit

        inline Interval functionRangeImplIntervalDomain(ExprPtr const& function, ExprPtr const& symbol, ExprPtr const& domain)
        {
#if DEBUG
            std::cout << "functionRangeImplIntervalDomain: " << toString(function) << ",\t" << toString(symbol)  << ",\t" << toString(domain) << std::endl;
#endif // DEBUG

            auto const domain_ = std::get<Interval>(*domain);
            auto const subs = [&] (ExprPtr const& dst) { return substitute(function, symbol >> dst); };
            auto const firstP = IntervalEnd{subs(domain_.first.first), domain_.first.second};
            auto const secondP = IntervalEnd{subs(domain_.second.first), domain_.second.second};
            auto const derivative = diff(function, symbol);
            auto const criticalPoints = solve(derivative, symbol, domain);
            auto min_ = evald(firstP.first) < evald(secondP.first) ? firstP : secondP;
            auto max_ = evald(firstP.first) < evald(secondP.first) ? secondP : firstP;
            // optimize me
            for (auto const& e : std::get<Set>(*criticalPoints))
            {
                auto const v = IntervalEnd{subs(e), true};
                min_ = evald(min_.first) < evald(v.first) ? min_ : v;
                max_ = evald(max_.first) > evald(v.first) ? max_ : v;
            }
            return Interval{min_, max_};
        }

        inline ExprPtr unionInterval(Interval const& lhs, Interval const& rhs)
        {
            if (lhs.first.first == nullptr && lhs.second.first == nullptr)
            {
                return std::make_shared<Expr const>(rhs);
            }

#if DEBUG
            std::cout << "unionInterval: " << toString(std::make_shared<Expr const>(lhs)) << ",\t" << toString(std::make_shared<Expr const>(rhs)) << std::endl;
#endif // DEBUG

            auto const realInterval = [](auto &&left, auto &&right)
            { return ds(left.at(ds(asDouble, _)), right.at(ds(asDouble, _))); };

            Id<IntervalEnd> iIEL1, iIER1, iIEL2, iIER2;
            return match(lhs, rhs)
            (
                pattern | ds(realInterval(iIEL1, iIER1), realInterval(iIEL2, iIER2)) = [&]
                {
                    auto const left = evald((*iIEL1).first) <= evald((*iIEL2).first) ? *iIEL1 : *iIEL2;
                    auto const right = evald((*iIER1).first) >= evald((*iIER2).first) ? *iIER1 : *iIER2;
                    if (evald((*iIEL1).first) <= evald((*iIER2).first) && evald((*iIEL2).first) <= evald((*iIER1).first))
                    {
                        return std::make_shared<Expr const>(Interval{left, right});
                    }
                    return false_;
                },
                pattern | _ = [&]
                {
                    throw std::logic_error{"Mismatch!"};
                    return std::make_shared<Expr const>(lhs);
                }
            );
        }


        inline Interval functionRangeImpl(ExprPtr const& function, ExprPtr const& symbol, ExprPtr const& domain)
        {
#if DEBUG
            std::cout << "functionRangeImpl: " << toString(function) << ",\t" << toString(symbol)  << ",\t" << toString(domain) << std::endl;
#endif // DEBUG

            Id<Union> iUnion;
            return match(*domain)
            (
                pattern | as<Interval>(_) = [&]
                {
                    return functionRangeImplIntervalDomain(function, symbol, domain);
                },
                pattern | as<SetOp>(as<Union>(iUnion)) = [&]
                {
                    Interval result;
                    for (auto&& e: *iUnion)
                    {
                        auto ret = unionInterval(result, functionRangeImpl(function, symbol, e));
                        Id<Interval> iInterval;
                        result = match(ret)
                        (
                            pattern | false_ = expr(result),
                            pattern | some(as<Interval>(iInterval)) = [&] {
                                return *iInterval;
                            },
                            pattern | _ = [&] {
                                throw std::runtime_error{"Mismatch in as<SetOp>(as<Union>(iUnion))!"};
                                return result;
                            }
                        );
                    }
                    return result;
                },
                pattern | _ = [&]
                {
                    throw std::logic_error{"Mismatch in functionRangeImpl!"};
                    return Interval{};
                }
            );
        }

        inline ExprPtr functionRange(ExprPtr const& function, ExprPtr const& symbol, ExprPtr const& domain)
        {
#if DEBUG
            std::cout << "functionRange: " << toString(function) << ",\t" << toString(symbol)  << ",\t" << toString(domain) << std::endl;
#endif // DEBUG

            Id<PieceWise> iPieceWise;
            auto result = match(*function)(
                pattern | as<PieceWise>(iPieceWise)   = [&] {
                    return std::accumulate((*iPieceWise).begin(), (*iPieceWise).end(), Interval{}, [&] (auto&& result, auto&& e)
                    {
                        auto const newDomain = intersect(solveInequation(e.second, symbol), domain);
                        if (equal(newDomain, false_))
                        {
                            return result;
                        }
                        auto ret = unionInterval(result, functionRangeImpl(e.first, symbol, newDomain));
                        Id<Interval> iInterval;
                        return match(ret)
                        (
                            pattern | false_ = expr(result),
                            pattern | some(as<Interval>(iInterval)) = [&] {
                                return *iInterval;
                            },
                            pattern | _ = [&] {
                                throw std::runtime_error{"Mismatch in as<SetOp>(as<Union>(iUnion))!"};
                                return result;
                            }
                        );
                    });
                },
                pattern | _ = [&]
                { return functionRangeImpl(function, symbol, domain); });
            if (result == Interval{})
            {
                return false_;
            }
            return std::make_shared<Expr const>(std::move(result));
        }
    } // namespace impl
} // namespace mathiu

#endif // FUNCTION_RANGE_H