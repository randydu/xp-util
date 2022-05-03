/**
 * Impl_Intfs.h
 *
 *  Created on: Feb 19, 2010
 *      Author: Randy
 *
 *  \file
 *  \brief Implementation of interface concept.
 */

#ifndef _XP_IMPL_INTFS_H_
#define _XP_IMPL_INTFS_H_

#include "class_util.h"
#include "intf_defs.h"
#include "on_exit.h"

#include <algorithm>
#include <assert.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace xp {

namespace detail {

struct QueryState : IQueryState {
private:
    std::unordered_set<void*> _searched;

public:
    virtual void addSearched(void* p) override
    {
        _searched.insert(p);
    }
    virtual bool isSearched(void* p) const override
    {
        return _searched.count(p);
    }
};
} // namespace detail


enum class ref_api_t {
    REF,
    UNREF,
    UNREF_NODELETE
};
using ref_monitor_t = std::function<void(IRefObj* obj, int /*count*/, ref_api_t api)>;

template <class T>
class TRefObj : public T
{
public:
    TRefObj() = default;

    using T::T; // gsl::C.52

    TRefObj(const TRefObj&) = delete;
    TRefObj& operator=(const TRefObj&) = delete;
    TRefObj(TRefObj&& other) = delete;
    TRefObj& operator=(TRefObj&& other) = delete;

    // IRefObj
    virtual void ref() override
    {
        if (_monitor)
            _monitor(this, _count, ref_api_t::REF);
        ++_count;
    }
    virtual void unref() override
    {
        if (_monitor)
            _monitor(this, _count, ref_api_t::UNREF);
        if (_count == 0)
            throw std::logic_error("unref() >> ref-count is already 0.");
        if (--_count == 0) {
            delete this;
        }
    }
    virtual void unrefNoDelete() override
    {
        if (_monitor)
            _monitor(this, _count, ref_api_t::UNREF_NODELETE);
        if (_count == 0)
            throw std::logic_error("::unrefNoDelete() >> ref-count is already 0.");
        --_count;
    }
    virtual int count() const override { return _count; }

    void setMonitor(ref_monitor_t monitor) { _monitor = monitor; }

protected:
    virtual ~TRefObj() = default; // protected destructor to enforce heap-based allocation.
private:
    int _count{0};
    ref_monitor_t _monitor;
};

template <typename T, typename... TArgs>
IRefObj* make_ref(TArgs&&... args)
{
    static_assert(std::is_base_of_v<IRefObj, T>);
    return new TRefObj<T>(std::forward<TArgs>(args)...);
}

/**
 * \class TInterface<>
 * \brief Implements IInterface
 *
 *  Usage:
 *
 *  - defines a new interface
 *  \code
 *  INTERFACE IHello : public IInterface {
 *     virtual void sayHello() = 0;
 *  };
 *	\endcode
 *  - Implementation
 *  \code
 *  class Impl_Hello : public IHello {
 *    public:
 *      void sayHello(){
 *         cout << "Hello World!" << endl;
 *      }
 *  };
 * \endcode
 *  - use it:
 *  \code
 *  auto_ref<IHello> hello(new TInterface<Impl_Hello>());
 *  hello->sayHello(); //say "Hello World!";
 *  \endcode
 *
 */

template <class T>
class TInterface : public TRefObj<T>
{
    using parent_t = TRefObj<T>;

public:
    TInterface() = default;

    using parent_t::parent_t; // gsl C.52

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        if (equalIID(iid, IID(T)) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface*)(this);
            return 0;
        }

        return 1;
    }

protected:
    virtual ~TInterface() = default; // no stack object
};

template <class T, class... S>
class TMultiInterface : public TRefObj<T>
{
    using parent_t = TRefObj<T>;

public:
    using parent_t::parent_t; // gsl C.52

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        if (match_iid<S...>(iid, retIntf)) {
            return 0;
        }
        if (equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface*)(this);
            return 0;
        }

        return 1;
    }

protected:
    virtual ~TMultiInterface() = default;

private:
    template <typename U, typename... V>
    bool match_iid(TIntfId iid, void** retIntf)
    {
        if (equalIID(iid, IID(U))) {
            this->ref();
            *retIntf = static_cast<U*>(this);
            return true;
        }
        if constexpr (sizeof...(V) > 0) {
            return match_iid<V...>(iid, retIntf);
        } else {
            return false;
        }
    }
};

template <typename T, typename... TArgs>
T* make_intf(TArgs&&... args)
{
    static_assert(std::is_base_of_v<IInterface, T>);

    if constexpr (std::is_constructible_v<T, TArgs...>) {
        return new T(std::forward<TArgs>(args)...);
    } else {
        return new TInterface<T>(std::forward<TArgs>(args)...);
    }
}


/**
 * \class TInterfaceEx<>
 * \brief Implements IInterfaceEx
 *
 *  Usage:
 *
 *  - defines a new interface
 *  \code
 *  INTERFACE IHello : public IInterfaceEx {
 *     DECLARE_IID(IHello);
 *
 *     virtual void sayHello() = 0;
 *  };
 *	\endcode
 *  - Implementation
 *  \code
 *  class Impl_Hello : public IHello {
 *    public:
 *      void sayHello(){
 *         cout << "Hello World!" << endl;
 *      }
 *  };
 * \endcode
 *  - use it:
 *  \code
 *
 *  IBus* bus; //initialized elsewhere
 *
 *  bus->connect(new TInterfaceEx<Impl_Hello>()); //bus connection.
 *
 *  IHello* hello;
 *  if(0 == bus->queryInterface("IHello", (void**)&hello){
 *    hello->sayHello(); //==> "Hello World!"
 *    hello->unref();
 *  }
 *  \endcode
 *
 *
 */
template <class T, bool check_iid = true>
class TInterfaceEx : public TRefObj<T>
{
    using parent_t = TRefObj<T>;

public:
    using parent_t::parent_t; // gsl C.52

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        detail::QueryState qst;
        return queryInterfaceEx(iid, retIntf, qst);
    }
    // IInterfaceEx
    virtual int queryInterfaceEx(TIntfId iid, void** retIntf, IQueryState& qst) override
    {
        if constexpr (check_iid) { // multi-interfaceex check by iteself; also fix ambiguous T::iid compiling issue when T inherits multiple interfaces.
            if (equalIID(iid, IID(T)) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
                this->ref();
                *retIntf = (IInterface*)(this);
                return 0;
            }
        }

        qst.addSearched(this);

        if (_bus) {
            if (!qst.isSearched(_bus)) {
                return _bus->queryInterfaceEx(iid, retIntf, qst);
            }
        }
        return 1;
    }
    virtual void setBus(IBus* bus) override
    {
        if (_bus != nullptr && bus != nullptr)
            throw std::logic_error("TInterfaceEx::_setBus() >> hosting bus already exists!");
        _bus = bus;
    }

    virtual void finish() override
    {
        if (!_cleared) {
            _cleared = true;
            onClear();
        }
    }

    // query if the apis should be disabled.
    virtual bool finished() const override
    {
        return _cleared;
    }
    virtual int getFinishPass() const override
    {
        return _finish_pass;
    }

protected:
    IBus* _bus{nullptr};
    bool _cleared{false}; // any apis should not be called any more
    int _finish_pass{1};

    virtual ~TInterfaceEx() = default;

    virtual void onClear() {} // called when the finish() is invokded. subclass may override this to release managed resources before destructor.
};

template <typename T, typename... TArgs>
T* make_intfx(TArgs&&... args)
{
    static_assert(std::is_base_of_v<IInterfaceEx, T>);

    if constexpr (std::is_constructible_v<T, TArgs...>) {
        return new T(std::forward<TArgs>(args)...);
    } else {
        return new TInterfaceEx<T>(std::forward<TArgs>(args)...);
    }
}

template <class T, class... S>
class TMultiInterfaceEx : public TInterfaceEx<T, false>
{
    using parent_t = TInterfaceEx<T, false>;

public:
    using parent_t::parent_t; // gsl C.52

    // First interface-ex provided
    // When connecting to bus, to avoid IInterfaceEx ambiguous due to multiple IInterfaceEx inheritance.
    // bus->connect(multifx->first_service());
    auto first_service()
    {
        using first_type = first_type_t<S...>;
        return static_cast<first_type*>(this);
    }

    using parent_t::queryInterface;

    virtual int queryInterfaceEx(TIntfId iid, void** retIntf, IQueryState& qst) override
    {
        if (match_iid<S...>(iid, retIntf)) {
            return 0;
        }

        parent_t* p = static_cast<parent_t*>(this);
        if (equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = p;
            return 0;
        }

        qst.addSearched(p);

        if (parent_t::_bus) {
            if (!qst.isSearched(parent_t::_bus)) {
                return parent_t::_bus->queryInterfaceEx(iid, retIntf, qst);
            }
        }
        return 1;
    }

protected:
    virtual ~TMultiInterfaceEx() = default;

private:
    template <typename U, typename... V>
    bool match_iid(TIntfId iid, void** retIntf)
    {
        if (equalIID(iid, IID(U))) {
            this->ref();
            *retIntf = static_cast<U*>(this);
            return true;
        }
        if constexpr (sizeof...(V) > 0) {
            return match_iid<V...>(iid, retIntf);
        } else {
            return false;
        }
    }
};

/**
 * @brief Base class for object implementing multiple interfaces
 *
 * @tparam T  first interface
 * @tparam S  [optional] other interfaces
 *
 *  All of the interfaces implemented by this class are clustered in a single
 *  group, you can navigate from one interface to another:
 *
 *   auto_ref<IA> pa; // we have a IA interface
 *   assert(pa);
 *   auto_ref<IB> pb = pa; //IB can be navigated from IA.
 *   assert(pb);
 *
 *
 * @code {.cpp}
 *   Interface declarations:
 *
 *   struct IFoo: IInterface {
 *        DECLARE_IID("foo-service");
 *        virtual void foo() = 0;
 *   struct IBar: IInterface {
 *        DECLARE_IID("bar-service");
 *        virtual void bar() = 0;
 *   };
 *
 *   Implementation:
 *
 *   class Foo : public TInterfaceBase<IFoo> {
 *       public:
 *           virtual void foo() override {}
 *   };
 *
 *   xp::auto_ref<IFoo> foo = new Foo();
 *
 *   class FooBar : public TInterfaceBase<IFoo, IBar> {
 *       public:
 *           virtual void foo() override {}
 *           virtual void bar() override {}
 *   };
 *
 *   xp::auto_ref<IFoo> foo = new FooBar();
 *   ...
 *   xp::auto_ref<IBar> bar = foo; //navigate from IFoo to IBar.
 *   CHECK(bar);
 *
 * @endcode
 *
 */
template <class T, class... S>
class TInterfaceBase : virtual public TRefObj<T>, virtual public S...
{
    constexpr void concept()
    {
        // T, S... are interfaces derived from IInterface only.
        static_assert(std::is_base_of_v<IInterface, T>);
        static_assert(!std::is_base_of_v<IInterfaceEx, T>);

        if constexpr (sizeof...(S) > 0) {
            static_assert((std::is_base_of_v<IInterface, S> && ...));
            static_assert(!(std::is_base_of_v<IInterfaceEx, S> && ...));
        }
    }

public:
    TInterfaceBase() = default;

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        if (equalIID(iid, IID(T)) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = static_cast<T*>(this);
            return 0;
        }

        if constexpr (sizeof...(S) > 0) {
            if (match_iid<S...>(iid, retIntf)) {
                return 0;
            }
        }

        return 1;
    }

protected:
    virtual ~TInterfaceBase() = default;

private:
    template <typename U, typename... V>
    bool match_iid(TIntfId iid, void** retIntf)
    {
        if (equalIID(iid, IID(U))) {
            this->ref();
            *retIntf = static_cast<U*>(this);
            return true;
        }
        if constexpr (sizeof...(V) > 0) {
            return match_iid<V...>(iid, retIntf);
        } else {
            return false;
        }
    }
};


/** Multi-Interface with a built-in bus connector (IInterfaceEx)
    Allow publish multiple interfaces connected to bus in a single class object.

    @code {.cpp}
   Interface declarations:

   struct IFoo: IInterface {
        DECLARE_IID("foo-service");
        virtual void foo() = 0;
   struct IBar: IInterface {
        DECLARE_IID("bar-service");
        virtual void bar() = 0;
   };

   Implementation:

   class FooBar : public TInterfaceExBase<IFoo, IBar> {
       public:
           virtual void foo() override {}
           virtual void bar() override {}
   };

   auto_ref bus = new TBus();
   bus->connect(new FooBar()); //publish IFoo & IBar
   auto_ref<IFoo> foo = bus;   //query the service from bus
   CHECK(foo);
   xp::auto_ref<IBar> bar = foo; //navigate from IFoo to IBar.
   CHECK(bar);

    @endcode


 */
template <class... S>
class TInterfaceExBase : virtual public TRefObj<IInterfaceEx>, virtual protected S...
{
    // S are interfaces derived from IInterface only.
    static_assert((std::is_base_of_v<IInterface, S> && ...));
    static_assert(!(std::is_base_of_v<IInterfaceEx, S> && ...));

public:
    TInterfaceExBase() = default;

    // IInterfaceEx
    virtual int queryInterfaceEx(TIntfId iid, void** retIntf, IQueryState& qst) override
    {
        if (equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterfaceEx*)(this);
            return 0;
        }

        if (match_iid<S...>(iid, retIntf)) {
            return 0;
        }

        qst.addSearched(this);

        if (_bus) {
            if (!qst.isSearched(_bus)) {
                return _bus->queryInterfaceEx(iid, retIntf, qst);
            }
        }
        return 1;
    }

    virtual void setBus(IBus* bus) override
    {
        if (_bus != nullptr && bus != nullptr)
            throw std::logic_error("TInterfaceEx::_setBus() >> hosting bus already exists!");
        _bus = bus;
    }

    virtual void finish() override
    {
        if (!_cleared) {
            _cleared = true;
            this->onClear();
        }
    }

    // query if the apis should be disabled.
    virtual bool finished() const override
    {
        return _cleared;
    }
    virtual int getFinishPass() const override
    {
        return _finish_pass;
    }

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        detail::QueryState qst;
        return queryInterfaceEx(iid, retIntf, qst);
    }

protected:
    IBus* _bus{nullptr};  // non-referenced
    bool _cleared{false}; // any apis should not be called any more
    int _finish_pass{1};

    virtual ~TInterfaceExBase() = default;

    virtual void onClear() {} // called when finish() is invokded.

private:
    template <typename U, typename... V>
    bool match_iid(TIntfId iid, void** retIntf)
    {
        if (equalIID(iid, IID(U))) {
            this->ref();
            *retIntf = static_cast<U*>(this);
            return true;
        }
        if constexpr (sizeof...(V) > 0) {
            return match_iid<V...>(iid, retIntf);
        } else {
            return false;
        }
    }
};


// IBus
class TBus : public TInterfaceEx<IBus, false>
{
public:
    explicit TBus(int busLevel = 0) : _level(busLevel) {}

    int total_intfs() const { return _intfs.size(); }
    int total_buses() const { return _buses.size(); }
    int total_siblings() const { return _siblings.size(); }

    // IBus
    [[nodiscard]] virtual bool connect(IInterfaceEx* intf) override;
    virtual void disconnect(IInterfaceEx* intf) override;

    virtual int level() const override
    {
        return _level;
    }
    virtual IBus* findFirstBusByLevel(int busLevel) const override;

    virtual void addSiblingBus(IBus* bus) override;
    virtual void removeSiblingBus(IBus* bus) override;

    virtual int queryInterfaceEx(TIntfId iid, void** retIntf, IQueryState& qst) override;

protected:
    int _level; // busLevel
    // IBus* _bus; //hosting bus with a more secure level ( _bus->level() <= this->level() )
    std::vector<IInterfaceEx*> _intfs;
    std::vector<IBus*> _buses;    // connected buses with less secure levels ( >= this->level() ), strong-referenced.
    std::vector<IBus*> _siblings; // bus with the same level as mine. (weak-referenced)

    virtual ~TBus()
    {
        reset();
    }
    virtual void onClear()
    {
        reset();
    }

    void reset();

private:
    enum {
        ACTIVE,
        CLEARING,
        CLEARED
    } _status{ACTIVE};

    void assert_api_not_closed() const;
};

} // namespace xp

#endif /* IMPL_INTFS_H_ */
