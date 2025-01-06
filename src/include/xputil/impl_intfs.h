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
#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <mutex>

#if defined(_MSC_VER)
// disable false positive warning C4250: inherits via dominance
#pragma warning(disable : 4250)
#endif

namespace xp {

namespace detail {

struct QueryState : IQueryState {
public:
    void addSearched(void* p) override
    {
        _searched.insert(p);
    }
    bool isSearched(void* p) const override
    {
        return _searched.count(p);
    }

private:
    std::unordered_set<void*> _searched{};
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
    void ref() override
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_monitor)
            _monitor(this, _count, ref_api_t::REF);
        ++_count;
    }
    void unref() override
    {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_monitor)
            _monitor(this, _count, ref_api_t::UNREF);
        if (_count == 0)
            throw std::logic_error("unref() >> ref-count is already 0.");
        if (--_count == 0) {
            lock.unlock(); // make sure the _mutex is unlocked while destroying object.
            delete this;
        }
    }
    void unrefNoDelete() override
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_monitor)
            _monitor(this, _count, ref_api_t::UNREF_NODELETE);
        if (_count == 0)
            throw std::logic_error("::unrefNoDelete() >> ref-count is already 0.");
        --_count;
    }
    int count() const override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _count;
    }

    void setMonitor(ref_monitor_t monitor)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _monitor = monitor;
    }

protected:
    ~TRefObj() override = default; // protected destructor to enforce heap-based allocation.
private:
    mutable std::mutex _mutex;
    int _count{0};            // GUARDED_BY(_mutex)
    ref_monitor_t _monitor{}; // GUARDED_BY(_mutex)
};

template <typename T, typename... TArgs>
constexpr xp::auto_ref<T> make_ref(TArgs&&... args)
{
    static_assert(std::is_base_of_v<IRefObj, T>);
    xp::auto_ref<T> r(new TRefObj<T>(std::forward<TArgs>(args)...));
    return r;
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

    TInterface(const TInterface&) = delete;
    TInterface(TInterface&&) = delete;
    TInterface& operator=(const TInterface&) = delete;
    TInterface& operator=(TInterface&&) = delete;

    using parent_t::parent_t; // gsl C.52

    // IInterface
    xp_error_code queryInterface(TIntfId iid, IInterface** retIntf) override
    {
        if (equalIID(iid, IID(T)) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface*)(this);
            return xp_error_code::OK;
        }

        return xp_error_code::INTF_NOT_RESOLVED;
    }

protected:
    ~TInterface() override = default; // no stack object
};

template <class T, class... S>
class TMultiInterface : public TRefObj<T>
{
    using parent_t = TRefObj<T>;

public:
    using parent_t::parent_t; // gsl C.52

    TMultiInterface(const TMultiInterface&) = delete;
    TMultiInterface(TMultiInterface&&) = delete;
    TMultiInterface& operator=(const TMultiInterface&) = delete;
    TMultiInterface& operator=(TMultiInterface&&) = delete;

    // IInterface
    xp_error_code queryInterface(TIntfId iid, IInterface** retIntf) override
    {
        if (match_iid<S...>(iid, retIntf)) {
            return xp_error_code::OK;
        }
        if (equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface*)(this);
            return xp_error_code::OK;
        }

        return xp_error_code::INTF_NOT_RESOLVED;
    }

protected:
    ~TMultiInterface() override = default;

private:
    template <typename U, typename... V>
    bool match_iid(TIntfId iid, IInterface** retIntf)
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
constexpr auto make_intf(TArgs&&... args)
{
    static_assert(std::is_base_of_v<IInterface, T>);

    if constexpr (std::is_constructible_v<T, TArgs...>) {
        return xp::auto_ref<T>(new T(std::forward<TArgs>(args)...));
    } else {
        return xp::auto_ref<TInterface<T>>(new TInterface<T>(std::forward<TArgs>(args)...));
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
 *  if(0 == bus->queryInterface("IHello", (IInterface**)&hello){
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

    TInterfaceEx(const TInterfaceEx&) = delete;
    TInterfaceEx(TInterfaceEx&&) = delete;
    TInterfaceEx& operator=(const TInterfaceEx&) = delete;
    TInterfaceEx& operator=(TInterfaceEx&&) = delete;
    // IInterface
    xp_error_code queryInterface(TIntfId iid, IInterface** retIntf) override
    {
        Expects(!_cleared);

        detail::QueryState qst;
        return queryInterfaceEx(iid, retIntf, qst);
    }
    // IInterfaceEx
    xp_error_code queryInterfaceEx(TIntfId iid, IInterface** retIntf, IQueryState& qst) override
    {
        if constexpr (check_iid) { // multi-interfaceex check by iteself; also fix ambiguous T::iid compiling issue when T inherits multiple interfaces.
            if (equalIID(iid, IID(T)) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
                this->ref();
                *retIntf = (IInterface*)(this);
                return xp_error_code::OK;
            }
        }

        qst.addSearched(this);

        return this->searchBus(iid, retIntf, qst);
    }
    void setBus(IBus* bus) override
    {
        if (_bus != nullptr && bus != nullptr)
            throw std::logic_error("TInterfaceEx::_setBus() >> hosting bus already exists!");
        _bus = bus;
    }

    void finish() override
    {
        if (!_cleared) {
            onClear();

            _bus = nullptr;
            _cleared = true;
        }
    }

protected:
    ~TInterfaceEx() override = default;
    virtual void onClear() {} // called when the finish() is invokded. subclass may override this to release managed resources before destructor.

    xp_error_code searchBus(TIntfId iid, IInterface** retIntf, IQueryState& qst)
    {
        return _bus ? resolve(_bus, iid, retIntf, qst) : xp_error_code::INTF_NOT_RESOLVED;
    }

    constexpr bool finished() const
    {
        return _cleared;
    }

private:
    IBus* _bus{nullptr};
    bool _cleared{false}; // any apis should not be called any more
};

template <typename T, typename... TArgs>
constexpr auto make_intfx(TArgs&&... args)
{
    static_assert(std::is_base_of_v<IInterfaceEx, T>);

    if constexpr (std::is_constructible_v<T, TArgs...>) {
        return xp::auto_ref<T>(new T(std::forward<TArgs>(args)...));
    } else {
        return xp::auto_ref<TInterfaceEx<T>>(new TInterfaceEx<T>(std::forward<TArgs>(args)...));
    }
}

template <class T, class... S>
class TMultiInterfaceEx : public TInterfaceEx<T, false>
{
    using parent_t = TInterfaceEx<T, false>;

public:
    using parent_t::parent_t; // gsl C.52

    TMultiInterfaceEx(const TMultiInterfaceEx&) = delete;
    TMultiInterfaceEx(TMultiInterfaceEx&&) = delete;
    TMultiInterfaceEx& operator=(const TMultiInterfaceEx&) = delete;
    TMultiInterfaceEx& operator=(TMultiInterfaceEx&&) = delete;

    // First interface-ex provided
    // When connecting to bus, to avoid IInterfaceEx ambiguous due to multiple IInterfaceEx inheritance.
    // bus->connect(multifx->first_service());
    auto first_service()
    {
        using first_type = first_type_t<S...>;
        return static_cast<first_type*>(this);
    }

    using parent_t::queryInterface;

    xp_error_code queryInterfaceEx(TIntfId iid, IInterface** retIntf, IQueryState& qst) override
    {
        if (match_iid<S...>(iid, retIntf)) {
            return xp_error_code::OK;
        }

        // parent_t* p = static_cast<parent_t*>(this);
        if (equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = first_service();
            return xp_error_code::OK;
        }

        qst.addSearched(this);

        return this->searchBus(iid, retIntf, qst);
    }

protected:
    ~TMultiInterfaceEx() override = default;

private:
    template <typename U, typename... V>
    constexpr bool match_iid(TIntfId iid, IInterface** retIntf)
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
    constexpr void concept_check()
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

    TInterfaceBase(const TInterfaceBase&) = delete;
    TInterfaceBase(TInterfaceBase&&) = delete;
    TInterfaceBase& operator=(const TInterfaceBase&) = delete;
    TInterfaceBase& operator=(TInterfaceBase&&) = delete;

    // IInterface
    xp_error_code queryInterface(TIntfId iid, IInterface** retIntf) override
    {
        if (equalIID(iid, IID(T)) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = static_cast<T*>(this);
            return xp_error_code::OK;
        }

        if constexpr (sizeof...(S) > 0) {
            if (match_iid<S...>(iid, retIntf)) {
                return xp_error_code::OK;
            }
        }

        return xp_error_code::INTF_NOT_RESOLVED;
    }

protected:
    ~TInterfaceBase() override = default;

private:
    template <typename U, typename... V>
    bool match_iid(TIntfId iid, IInterface** retIntf)
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
           void foo() override {}
           void bar() override {}
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

    TInterfaceExBase(const TInterfaceExBase&) = delete;
    TInterfaceExBase(TInterfaceExBase&&) = delete;
    TInterfaceExBase& operator=(const TInterfaceExBase&) = delete;
    TInterfaceExBase& operator=(TInterfaceExBase&&) = delete;

    // IInterfaceEx
    xp_error_code queryInterfaceEx(TIntfId iid, IInterface** retIntf, IQueryState& qst) override
    {
        if (equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterfaceEx*)(this);
            return xp_error_code::OK;
        }

        if (match_iid<S...>(iid, retIntf)) {
            return xp_error_code::OK;
        }

        qst.addSearched(this);

        return _bus ? resolve(_bus, iid, retIntf, qst) : xp_error_code::INTF_NOT_RESOLVED;
    }

    void setBus(IBus* bus) override
    {
        if (_bus != nullptr && bus != nullptr)
            throw std::logic_error("TInterfaceEx::_setBus() >> hosting bus already exists!");
        _bus = bus;
    }

    void finish() override
    {
        if (!_cleared) {
            _cleared = true;
            _bus = nullptr;
            this->onClear();
        }
    }

    // IInterface
    xp_error_code queryInterface(TIntfId iid, IInterface** retIntf) override
    {
        Expects(!_cleared);

        detail::QueryState qst;
        return queryInterfaceEx(iid, retIntf, qst);
    }

protected:
    ~TInterfaceExBase() override = default;

    virtual void onClear() {} // called when finish() is invokded.

private:
    IBus* _bus{nullptr};  // non-referenced
    bool _cleared{false}; // any apis should not be called any more

    template <typename U, typename... V>
    bool match_iid(TIntfId iid, IInterface** retIntf)
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
    TBus(const TBus&) = delete;
    TBus& operator=(const TBus&) = delete;
    TBus(TBus&&) = delete;
    TBus& operator=(TBus&&) = delete;

    auto total_intfs() const
    {
        std::lock_guard lock(_mutex);
        return _intfs.size();
    }
    auto total_buses() const
    {
        std::lock_guard lock(_mutex);
        return _buses.size();
    }
    auto total_siblings() const
    {
        std::lock_guard lock(_mutex);
        return _siblings.size();
    }

    // IBus
    [[nodiscard]] bool connect(gsl::not_null<IInterfaceEx*> intf, int order = 0) override
    {
        Expects(!this->finished());

        if (intf == this) return false; // no loop-back.

        std::lock_guard lock(_mutex);

        IBus* bus{nullptr};
        detail::QueryState qst;
        if (intf->queryInterfaceEx(IID_IBUS, (IInterface**)&bus, qst) == xp_error_code::OK) { // NOLINT
            ON_EXIT(bus->unref());                                                            // balance queryInterface

            const int level = bus->level();
            if (level > _level) {
                // do not allow duplicated buses
                if (auto it = std::find(_buses.begin(), _buses.end(), bus); it != _buses.end())
                    return false;

                // strong reference only for different level.
                bus->ref();
                _buses.push_back(bus);

                std::sort(_buses.begin(), _buses.end(), [](auto x, auto y) { return x->level() < y->level(); });
                return true;
            }

            if (level == _level) {
                if (bus->count() == 1) {
                    // sibing bus is not referenced outside.
                    //
                    // Because we only keep a weak reference to a sibling bus, which will be destroyed if there is no external
                    // reference lock.
                    // ex:  bus0->connect(new TBus(0));
                    //
                    // we could unrefNoDelete it to keep it alive after return, but it might lead to memory leakge if it is not
                    // referenced later.
                    // so we can avoid this kind of usage before bad things happens.
                    return false;
                }

                // no loop-back
                if (bus == this)
                    return false;

                // do not allow duplicated buses
                if (auto it = std::find(_siblings.begin(), _siblings.end(), bus); it != _siblings.end())
                    return false;

                // weak reference only for sibling bus to avoid reference deadlock, remove when being destroyed.
                _siblings.insert(bus);
                // sibling bus, mutual connection
                bus->addSiblingBus(this);

                return true;
            }

            // bus level smaller than mine, connection failure.
            return false;
        }

        // no duplicated interfaces
        if (auto it = std::find_if(_intfs.begin(), _intfs.end(), [intf](const auto& x) { return x.second == intf; }); it != _intfs.end())
            return false;

        intf->ref();
        _intfs.emplace_back(order, intf);
        intf->setBus(this);
        return true;
    }

    void disconnect(gsl::not_null<IInterfaceEx*> intf) override
    {
        std::lock_guard lock(_mutex);
        Expects(!this->finished());

        // interfaces first
        if (auto it = std::find_if(_intfs.begin(), _intfs.end(), [intf](const auto& x) { return x.second == intf; }); it != _intfs.end()) {
            _intfs.erase(it);
            intf->setBus(nullptr);
            intf->unref();
            return;
        }
        // buses later
        if (auto it = find(_buses.begin(), _buses.end(), intf); it != _buses.end()) {
            _buses.erase(it);
            intf->unref();
            return;
        }

        if (intf->iid() == IBus::iid()) {
            removeSiblingBus(intf->cast<IBus>());
        }
    }

    int level() const override
    {
        return _level;
    }
    IBus* findFirstBusByLevel(int busLevel) override
    {
        Expects(!this->finished());

        if (busLevel < _level)
            return nullptr;

        if (_level == busLevel)
            return this;

        std::lock_guard lock(_mutex);
        for (auto bus : _buses) {
            if (auto p = bus->findFirstBusByLevel(busLevel); p) return p;
        }

        for (auto bus : _siblings) {
            if (auto p = bus->findFirstBusByLevel(busLevel); p) return p;
        }

        return nullptr;
    }


    void addSiblingBus(gsl::not_null<IBus*> bus) override
    {
        std::lock_guard lock(_mutex);
        Expects(!this->finished());

        _siblings.insert(bus);
    }

    void removeSiblingBus(gsl::not_null<IBus*> bus) override
    {
        std::lock_guard lock(_mutex);
        Expects(!this->finished());

        _siblings.erase(bus);
    }

    xp_error_code queryInterfaceEx(TIntfId iid, IInterface** retIntf, IQueryState& qst) override
    {
        Expects(retIntf);
        *retIntf = nullptr;

        if (equalIID(iid, IID_IBUS) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            *retIntf = this;
            this->ref();
            return xp_error_code::OK;
        }

        std::lock_guard lock(_mutex);

        qst.addSearched(this);

        // scanning interfaces in my slots
        for (auto [_, intf] : _intfs) {
            if (resolve(intf, iid, retIntf, qst) == xp_error_code::OK) return xp_error_code::OK;
        }
        // scan sibling buses
        for (auto bus : _siblings) {
            if (resolve(bus, iid, retIntf, qst) == xp_error_code::OK) return xp_error_code::OK;
        }
        // scanning connected upper-level/less-secure buses
        for (auto bus : _buses) {
            if (resolve(bus, iid, retIntf, qst) == xp_error_code::OK) return xp_error_code::OK;
        }

        return xp_error_code::INTF_NOT_RESOLVED;
    }

protected:
    ~TBus() override
    {
        if (!this->finished()) reset();
    }

private:
    int _level; // busLevel
    mutable std::recursive_mutex _mutex;

    // IBus* _bus; //hosting bus with a more secure level ( _bus->level() <= this->level() )
    std::vector<std::pair<int, IInterfaceEx*>> _intfs;
    std::vector<IBus*> _buses{};           // connected buses with less secure levels ( >= this->level() ), strong-referenced.
    std::unordered_set<IBus*> _siblings{}; // bus with the same level as mine. (weak-referenced)

    void onClear() override
    {
        std::lock_guard lock(_mutex);
        reset();
    }

    void reset()
    {
        std::lock_guard lock(_mutex);
        Expects(!this->finished());

        for (auto p : _siblings) {
            p->removeSiblingBus(this);
            // sibling bus not affected by my clearance.
            // p->finish();
        }
        _siblings.clear();

        // explicitly pass-ordered resource release
        // for the same pass, the later installed interface is released first.
        constexpr int max_clear_pass = 3;
        for (int pass = 0; pass < max_clear_pass; pass++) {
            for (auto it = _intfs.rbegin(); it != _intfs.rend(); ++it) {
                auto [order, intf] = *it;
                if (pass == order) {
                    intf->finish();
                }
            }
        }
        for (auto [_, intf] : _intfs) {
            intf->setBus(nullptr);
            intf->unref();
        }
        _intfs.clear();

        for (std::vector<IBus*>::reverse_iterator it = _buses.rbegin(); it != _buses.rend(); ++it) {
            IBus* bus = *it;
            bus->finish();
            bus->setBus(nullptr);
            bus->unref();
        }
        _buses.clear();
    }
};

} // namespace xp

#endif /* IMPL_INTFS_H_ */
