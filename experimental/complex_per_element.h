#pragma once
#include <cstddef>
#include <cassert>
#include <utility>
#include "meta.h"
#include "defines.h"

//LEGACY
#include <type_traits>
namespace jot
{
    using std::true_type;
    using std::false_type;

    template<class From, class To>
    struct custom_cast : false_type {};

    template<class From, class To>
    struct custom_explicit_cast : false_type {};

    template<class Class, class ...Args>
    struct custom_invoke : false_type {};

    template<class Class, class ...Args>
    struct custom_construct : false_type {};

    template<class Class, class To>
    struct custom_assign : false_type {};

    template<class Class>
    struct custom_destruct : false_type {};

    //REMOVE
    template<class From, class To>
    concept custom_castable = custom_cast<From, To>::value;

    template<class From, class To>
    concept custom_explicit_castable = custom_explicit_cast<From, To>::value;

    template<class Class, class ...Args>
    concept custom_invokable = custom_invoke<Class, Args...>::value;

    template<class Class, class ...Args>
    concept custom_constructible = custom_construct<Class, Args...>::value;

    template<class Class, class To>
    concept custom_assignable = custom_assign<Class, To>::value;

    template<class Class>
    concept custom_destructible = custom_destruct<Class>::value;
}

namespace jot
{
    namespace per_elem
    {
        struct DirectExpressionTag {};
        struct InfiniteExpressionTag {};

        template<class ExprAccess, class DataT>
        struct Expr
        {
            using Access = ExprAccess;
            using Data = DataT;

            Data data;

            using ExecuteT = decltype(Access::execute(data));
            using CheckT = decltype(Access::check(data));
            using AdvanceT = decltype(Access::advance(data));
            using ApplyT = decltype(Access::apply(data));
            using HookT = decltype(Access::hook(data));

            template<class T> 
                requires custom_castable<Expr, T>
            constexpr operator T() const {return custom_cast<Expr, T>::apply(*this);}

            template<class To>
                requires custom_assignable<Expr, To>
            proc operator=(const To& to) {return custom_assign<Expr, To>::apply(*this, to);}

            ~Expr()
            {
                Access::apply(data);
            }
        };

        template <typename T>
        concept Expression = requires(T op)
        {
            typename T::Data;
            typename T::Access;

            T::Access::execute(op.data);
            T::Access::check(op.data);
            T::Access::advance(op.data);
            T::Access::apply(op.data);
        };

        template<Expression T> proc execute(T& op) -> T::ExecuteT       { return T::Access::execute(op.data); };
        template<Expression T> proc check(T& op) -> T::CheckT           { return T::Access::check(op.data); };
        template<Expression T> proc advance(T& op) -> T::AdvanceT       { return T::Access::advance(op.data); };
        template<Expression T> proc apply(T& op) -> T::ApplyT           { return T::Access::apply(op.data); };
        template<Expression T> proc hook(T& op) -> T::ApplyT            { return T::Access::hook(op.data); };

        template <Expression Left, Expression Right> 
        struct BinaryData { 
            Left left; 
            Right right; 
            bool auto_apply = true;
        }; 

        template <Expression Left> 
        struct UnaryData { 
            Left left; 
            bool auto_apply = true;
        }; 

        template <class Data, class Executor, bool is_pure> 
        struct BinaryExprAccessor
        {
            static proc check(Data& self) {
                per_elem::check(self.right);
                return per_elem::check(self.left); 
            }

            static proc advance(Data& self){
                per_elem::advance(self.left); 
                per_elem::advance(self.right);
            }; 

            static proc execute(Data& self) -> decltype(Executor::execute(self.left, self.right)) {
                return Executor::execute(self.left, self.right);
            }

            static proc hook(Data& self) {
                self.auto_apply = false;
            }

            static proc apply(Data& self)
            { 
                if(!self.auto_apply)
                    return;

                if constexpr(is_pure)
                { 
                    per_elem::apply(self.left);
                    per_elem::apply(self.right); 
                }
                else
                {
                    while(check(self))
                    {
                        execute(self);
                        advance(self);
                    }
                }
            }; 
        };

        template <class Data, class Executor, bool is_pure> 
        struct UnaryExprAccessor
        {
            static proc check(Data& self)   { return per_elem::check(self.left); }
            static proc advance(Data& self) { return per_elem::advance(self.left); }; 

            static proc hook(Data& self) {
                self.auto_apply = false;
            }

            static proc execute(Data& self) -> decltype(Executor::execute(self.left)) {
                return Executor::execute(self.left);
            }

            static proc apply(Data& self)
            { 
                if(!self.auto_apply)
                    return;

                if constexpr(is_pure)
                    per_elem::apply(self.left);
                else
                    while(check(self))
                    {
                        execute(self);
                        advance(self);
                    }
            }; 
        };

        template<class Executor, bool is_expr_pure, Expression Left, Expression Right> 
        func make_bin_expr(const Left& left, const Right& right)
        { 
            using Data = BinaryData<Left, Right>;

            Data self{{left.data}, {right.data}};
            hook(self.left);
            hook(self.right);

            return Expr<BinaryExprAccessor<Data, Executor, is_expr_pure>, Data>{self}; 
        } 

        template<class Executor, bool is_expr_pure, Expression Left> 
        func make_unary_expr(const Left& left)
        { 
            using Data = UnaryData<Left>;

            Data self{{left.data}};
            hook(self.left);

            return Expr<UnaryExprAccessor<Data, Executor, is_expr_pure>, Data>{self}; 
        } 

        #define define_bin_op(op, is_pure) \
            template<Expression Left, Expression Right> \
            proc operator op(const Left& left, const Right& right) \
                requires requires(Left left, Right right) {per_elem::execute(left) op per_elem::execute(right);} \
            { \
                struct Executor {static proc execute(Left& left, Right& right) -> decltype(per_elem::execute(left) op per_elem::execute(right)) { \
                    return per_elem::execute(left) op per_elem::execute(right); \
                }}; \
                return make_bin_expr<Executor, is_pure>(left, right); \
            } \


        #define define_unary_op(prefix, postfix, is_pure) \
            template<Expression Left> \
            proc operator prefix postfix(const Left& left) \
                requires requires(Left left) { prefix per_elem::execute(left) postfix;} \
            { \
                struct Executor {static proc execute(Left& left) -> decltype( prefix per_elem::execute(left) postfix) { \
                    return prefix per_elem::execute(left) postfix; \
                }}; \
                return make_unary_expr<Executor, is_pure>(left); \
            } \


        define_bin_op(+, true);
        define_bin_op(-, true);
        define_bin_op(*, true);
        define_bin_op(/, true);
        define_bin_op(%, true);
        define_bin_op(&, true);
        define_bin_op(|, true);
        define_bin_op(^, true);
        define_bin_op(>>, true);
        define_bin_op(<<, true);

        define_bin_op(||, true);
        define_bin_op(&&, true);

        define_bin_op(==, true);
        define_bin_op(!=, true);
        define_bin_op(>, true);
        define_bin_op(<, true);
        define_bin_op(>=, true);
        define_bin_op(<=, true);
        define_bin_op(<=>, true);

        define_bin_op(+=, false);
        define_bin_op(-=, false);
        define_bin_op(*=, false);
        define_bin_op(/=, false);
        define_bin_op(%=, false);
        define_bin_op(&=, false);
        define_bin_op(|=, false);
        define_bin_op(^=, false);
        define_bin_op(<<=, false);
        define_bin_op(>>=, false);

        define_unary_op(+,, true);
        define_unary_op(-,, true);
        define_unary_op(~,, true);
        define_unary_op(!,, true);

        define_unary_op(++,, false);
        define_unary_op(--,, false);
        define_unary_op(,++, false);
        define_unary_op(,--, false);

        #undef define_bin_op
        #undef define_unary_op

        template <typename Range>
        concept EndedRange = requires(Range r) {r.end;};

        template <class Range, class It>
        struct DirectExpr
        {
            using Ref = typename std::iterator_traits<It>::reference;
            using Val = typename std::iterator_traits<It>::value_type;

            static proc advance (Range& range) { ++range.begin; };
            static proc execute (const Range& range) -> Val {return *range.begin;};
            static proc execute (Range& range) -> Ref {return *range.begin;};
            static proc apply   (const Range&) {};

            static proc check   (const Range& range) requires EndedRange<Range> { return range.begin != range.end; };
            static proc check   (const Range& range) requires !EndedRange<Range> { return true; };
            static proc hook    (const Range& range) {};
        };
    }

    template<typename Expr>
    concept PerElement = per_elem::Expression<Expr>;

    template<typename It>
    func to_per_element(It begin) 
    {
        using namespace per_elem;
        struct Range {
            It begin; 
            using Pure = true_type; 
            using Tag = type_collection<DirectExpressionTag, InfiniteExpressionTag>;
        };

        return Expr<DirectExpr<Range, It>, Range>{{begin}};
    }

    template<typename It>
    func to_per_element(It begin, It end) 
    {
        using namespace per_elem;
        struct Range {
            It begin; 
            It end; 
            using Pure = true_type; 
            using Tag = DirectExpressionTag;
        };

        return Expr<DirectExpr<Range, It>, Range>{{begin, end}};
    }

    template<PerElement Per>
    struct PerElementIter
    {
        using Val = typename Per::ExecuteT;

        Per& of;
        func operator !=(const PerElementIter&) const {return check(of);}
        func operator !=(const PerElementIter&) {return check(of);}

        func operator*() const -> Val {return execute(of);}
        func operator*() -> Val {return execute(of);}

        func operator++() const {return advance(of);}
        func operator++() {return advance(of);}
    };

    template<PerElement Per>
    struct CPerElementIter
    {
        using Val = typename Per::ExecuteT;

        const Per& of;
        func operator !=(const CPerElementIter&) const {return check(of);}
        func operator*() -> Val const {return execute(of);}
        func operator++() -> Val const {return advance(of);}
    };
}

namespace std
{
    func begin(jot::PerElement auto& op)           {return jot::PerElementIter{op};}
    func begin(const jot::PerElement auto& op)     {return jot::CPerElementIter{op};}

    func end(jot::PerElement auto& op)             {return jot::PerElementIter{op};}
    func end(const jot::PerElement auto& op)       {return jot::CPerElementIter{op};}

    func cbegin(const jot::PerElement auto& op)    {return jot::CPerElementIter{op};}
    func cend(const jot::PerElement auto& op)      {return jot::CPerElementIter{op};}

    func size(const jot::PerElement auto& op) {
        size_t size = 0;
        apply(op, [&](let&){ size++; });
        return size;
    }
}

namespace jot::per_elem
{
    template<Expression Left, Expression Right> 
    //requires requires(Left left, Right right) {execute(left) = execute(right);} 
    struct custom_assign<Left, Right> : true_type
    {
        pure static proc apply(const Left& left, const Right& right) 
        { 
            struct Executor {static proc execute(Left& left, Right& right) -> decltype(per_elem::execute(left) = per_elem::execute(right)) {
                return per_elem::execute(left) = per_elem::execute(right);
            }};
            return make_bin_expr<Executor, false>(left, right);
        } 
    };

    /*template<Expression Left, class... Args> 
    requires std::is_invocable_v<typename Left::ExecuteT, Args...>
    struct custom_invoke<Left, Args...> : true_type
    {
    pure static proc apply(const Left& left, Args&&... args) 
    { 
    using namespace std;
    struct Executor {static proc execute(const Left& left, Args&&... args) -> decltype(apply(forward<Left::ExecuteT>(per_elem::execute(left)), forward<Args>(args)...)) {
    return apply(forward<Left::ExecuteT>(per_elem::execute(left)), forward<Args>(args)...);
    }};
    return make_unary_expr<Executor, false>(left);
    } 
    };*/

    template<Expression From>
        requires requires(From from)
    { 
        { execute(from) } -> std::convertible_to<bool>;
    }
    struct custom_cast<From, bool> : true_type
    {
        pure static proc apply(From from) -> bool
        {
            return std::all_of(std::begin(from), std::end(from), [](mut&& val){
                return cast(bool) val;
                });
        }
    };
}

#include "undefs.h"