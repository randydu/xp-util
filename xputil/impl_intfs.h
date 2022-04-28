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

#include "intf_defs.h"
#include "on_exit.h"

#include <algorithm>
#include <assert.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace xp {

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
    template <typename... Ts>
    TRefObj(Ts&&... args) : T(std::forward<Ts>(args)...)
    {
    }
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
protected:
    virtual ~TInterface() = default;

public:
    template <typename... Ts>
    TInterface(Ts&&... args) : TRefObj<T>(std::forward<Ts>(args)...)
    {
    }

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        if (equalIID(iid, T::iid) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface*)(this);
            return 0;
        }

        return 1;
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

class TQueryState : public IQueryState
{
private:
    std::vector<IInterfaceEx*> _searched;

public:
    virtual void addSearched(IInterfaceEx* p) override
    {
        p->ref();
        _searched.push_back(p);
    }
    virtual bool isSearched(IInterfaceEx* p) const override
    {
        return std::find(_searched.cbegin(), _searched.cend(), p) != _searched.cend();
    }

    virtual ~TQueryState()
    {
        for (auto e : _searched)
            e->unref();
    }
};

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
template <class T>
class TInterfaceEx : public TRefObj<T>
{
private:
    // Release internal resources
    void clear()
    {
        if (!_cleared) {
            _cleared = true;
            onClear();
        }
    }

protected:
    IBus* _bus{nullptr};
    bool _cleared{false}; // any apis should not be called any more
    int _finish_pass{1};

    virtual ~TInterfaceEx()
    {
        // might not has been connected with any bus
        // assert((_bus == NULL)&& "TInterfaceEx::~TInterfaceEx >> should has been unplugged from hub!");
        clear();
    }

    // sub-class should override this method to release internal resources
    virtual void onClear() {}

public:
    template <typename... Ts>
    TInterfaceEx(Ts&&... args) : TRefObj<T>(std::forward<Ts>(args)...)
    {
    }

    // IInterfaceEx
    _INTERNAL_ virtual int _queryInterface(TIntfId iid, void** retIntf, IQueryState& qst) override
    {
        if (equalIID(iid, T::iid) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface*)(this);
            return 0;
        }

        qst.addSearched(this);

        if (_bus) {
            if (!qst.isSearched(_bus)) {
                return _bus->_queryInterface(iid, retIntf, qst);
            }
        }
        return 1;
    }
    _INTERNAL_ virtual void _setBus(IBus* bus) override
    {
        if (_bus != nullptr && bus != nullptr)
            throw std::logic_error("TInterfaceEx::_setBus() >> hosting bus already exists!");
        _bus = bus;
    }

    virtual void finish() override
    {
        clear();
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
        TQueryState qst;
        return _queryInterface(iid, retIntf, qst);
    }
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

#define BEGIN_INTERFACES          \
public:                           \
    bool supportIntf(TIntfId iid) \
    {
#define IMPL_INTERFACE(intf)          \
    {                                 \
        if (equalIID(iid, intf::iid)) \
            return true;              \
    }

#define END_INTERFACES \
    return false;      \
    }

/*
template <class T>
class TMultiInterfaceEx : public T {
  protected:
    IBus *_bus;

    virtual ~TMultiInterfaceEx() = default;

  public:
    template <typename... Ts>
    TMultiInterfaceEx(Ts... args) : T(args...), _bus(nullptr), _count(0) {}

    //unit-test only

    //IInterfaceEx
    _INTERNAL_ virtual int _queryInterface(TIntfId iid, void **retIntf, IQueryState &qst) override {
        if (T::supportIntf(iid) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
            this->ref();
            *retIntf = (IInterface *)(this);
            return 0;
        }

        qst.addSearched(this);

        if (_bus) {
            if (!qst.isSearched(_bus)) {
                return _bus->_queryInterface(iid, retIntf, qst);
            }
        }
        return 1;
    }

    _INTERNAL_ virtual void _setBus(IBus *bus) override {
        if (_bus != nullptr && bus != nullptr)
            throw std::logic_error("TMultiInterfaceEx::_setBus() >> hosting bus already exists!");
        _bus = bus;
    }

    virtual void finish() override {}
    virtual bool finished() override {}
    virtual int getFinishPass() const override { return 1; }

    //IInterface
    virtual int queryInterface(TIntfId iid, void **retIntf) override {
        TQueryState qst;
        return _queryInterface(iid, retIntf, qst);
    }

    //IRefObj
    IMPL_REFOBJ(TMultiInterfaceEx);
};
*/

// IBus
class TBus : public TRefObj<IBus>
{
private:
    enum {
        ACTIVE,
        CLEARING,
        CLEARED
    } _status{ACTIVE};

    void clear();
    void assert_api_not_closed() const;

protected:
    int _level; // busLevel
    // IBus* _bus; //hosting bus with a more secure level ( _bus->level() <= this->level() )
    std::vector<IInterfaceEx*> _intfs;
    std::vector<IBus*> _buses;    // connected buses with less secure levels ( >= this->level() ), strong-referenced.
    std::vector<IBus*> _siblings; // bus with the same level as mine. (weak-referenced)

    virtual ~TBus() { clear(); }

public:
    TBus(int busLevel) : _level(busLevel) {}

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

    // IInterfaceEx
    virtual int _queryInterface(TIntfId iid, void** retIntf, IQueryState& qst) override;
    virtual void _setBus(IBus* bus) override
    { /* unused */
    }

    virtual int getFinishPass() const override { return 0; }
    virtual void finish() override
    {
        clear();
    }
    virtual bool finished() const override
    {
        return _status == CLEARED;
    }

    // IInterface
    virtual int queryInterface(TIntfId iid, void** retIntf) override
    {
        TQueryState qst;
        return _queryInterface(iid, retIntf, qst);
    }
};

} // namespace xp

#endif /* IMPL_INTFS_H_ */
