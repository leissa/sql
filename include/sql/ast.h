#pragma once

#include <memory>
#include <ostream>
#include <unordered_set>

#include "sql/loc.h"
#include "sql/sym.h"

namespace sql {

template<class T>
using Ptr = std::unique_ptr<T>;

template<class T, class... Args>
Ptr<T> mk(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

using Vars = std::unordered_set<Sym>;

/// Base class for all @p Exp%ressions.
class Exp {
public:
    Exp(Loc loc)
        : loc_(loc)
    {}
    virtual ~Exp() {}

    Loc loc() const { return loc_; }
    void dump() const;

    /// Stream to @p o.
    virtual std::ostream& stream(std::ostream& o) const = 0;

private:
    Loc loc_;
};

/// A @p Var%iable with a @p name.
class Var : public Exp {
public:
    Var(Loc loc, Sym name)
        : Exp(loc)
        , name_(name)
    {}

    Sym name() const { return name_; }

    std::ostream& stream(std::ostream& o) const override;

private:
    Sym name_;
};

/// @p Lam%bda abstraction of the form (<code>lam</code> @p binder(). @p body()).
class Lam : public Exp {
public:
    Lam(Loc loc, Sym binder, Ptr<Exp>&& body)
        : Exp(loc)
        , binder_(binder)
        , body_(std::move(body))
    {}

    Sym binder() const { return binder_; }
    const Exp* body() const { return body_.get(); }

    std::ostream& stream(std::ostream& o) const override;

private:
    static int counter_;

    Sym binder_;
    Ptr<Exp> body_;
};

/// Function @p App%lication of the form (@p callee() @p arg()).
class App : public Exp {
public:
    App(Loc loc, Ptr<Exp>&& callee, Ptr<Exp>&& arg, bool let = false)
        : Exp(loc)
        , callee_(std::move(callee))
        , arg_(std::move(arg))
        , let_(let)
    {}

    const Exp* callee() const { return callee_.get(); }
    const Exp* arg() const { return arg_.get(); }
    const Lam* isa_let() const { return let_ ? (Lam*) callee() : nullptr; }

    std::ostream& stream(std::ostream&) const override;

private:
    Ptr<Exp> callee_;
    Ptr<Exp> arg_;
    bool let_;
};

/// The @p Err%or @p Exp%ression is a dummy that does nothing and will only be constructed during parse errors.
class Err : public Exp {
public:
    Err(Loc loc)
        : Exp(loc)
    {}

    std::ostream& stream(std::ostream& o) const override;
};

}
