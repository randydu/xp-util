#ifndef _XP_INTF_DEFS_H_
#define _XP_INTF_DEFS_H_

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <unordered_set>

#if defined(_WIN_)
#include <windows.h>
#endif

#include "class_attr.h"

#include <gsl/pointers>

namespace xp {
/**
 * \file
 * \brief Interface Bus Support
 *
 * Here is a simple yet powerful COM-style interface engine implemented from scratch to
 * support flexible cross-module interface browsing and integration.
 *
 * Features:
 *
 * -# Interface browsing;
 * -# Interfaces can be aggregated on the fly via "Interface Bus";
 * -# Interface Bus has a built-in security (bus level) control.
 * 	  Interfaces (containing more secure apis) hosted on a low level bus can discover
 * 	  the interfaces hosted on high level buses.
 */

/**
 * \typedef TIntfId
 * \brief The data type of interface identifier.
 *
 * Here we use a string instead of a GUID for more good code readability, it of course can be
 * a classical UUID string representation.
 */

// Calculate interface-id from a string
inline auto calc_iid(gsl::not_null<const char*> id_str)
{
    return std::hash<std::string>{}(std::string(id_str));
}
using TIntfId = decltype(calc_iid(""));

/**
 * \def DECLARE_IID
 * \brief defines the interface identifier in an implementation class.
 * \sa TInterface
 * \sa TInterfaceEx
 */
//#define DECLARE_IID(x) static inline xp::TIntfId iid() {return x;}
//#define DECLARE_IID(x) constexpr static auto iid = x


#define CALC_IID(x) xp::calc_iid(x)

#define DECLARE_IID(x)                   \
    inline static auto iid()             \
    {                                    \
        static auto h = xp::calc_iid(x); \
        return h;                        \
    }


/**
 * \def IID
 * \brief extracts interface id from an interface name.
 */
#define IID(intf) intf::iid()

/**
 * \fn bool equalIID(const TIntfId id1, const TIntfId id2);
 * \brief tests if two IIDs are equal.
 */
constexpr bool equalIID(const TIntfId id1, const TIntfId id2)
{
    return id1 == id2;
}

#ifndef INTERFACE
#define INTERFACE struct
#endif

class IRefObj
{
public:
    /**
     * increase reference count
     */
    virtual void ref() = 0;
    /**
     * decrease reference count, try destroying instance once reference count reaches zero.
     */
    virtual void unref() = 0;
    /**
     * decrease reference count, but do not destroy instance when reference count reaches zero.
     */
    virtual void unrefNoDelete() = 0;

    /**
     * returns reference count
     */
    virtual int count() const = 0;

protected:
    virtual ~IRefObj() = default; // heap-based only!
};


enum class xp_error_code : int {
    OK = 0,
    INTF_NOT_RESOLVED = 1,
};

/**
 * \interface IInterface
 * \brief root of all non-extensible interfaces
 *
 * It's platform independent and Windows COM interface can be converted to IInterface via an interface adapter.
 */
struct IInterface : virtual public IRefObj {
    DECLARE_IID("B4FF784E-2DDA-4CA2-BC84-4AAD35FCAAF3");

    /**
     *	Interface browsing, returns 0 if successful, non-zero error code if fails.
     */
    virtual xp_error_code queryInterface(TIntfId iid, void** retIntf) = 0;

    // helper: detect if another interface is accessible
    bool supports(TIntfId iid)
    {
        IInterface* intf(nullptr);
        if (this->queryInterface(iid, (void**)&intf) == xp_error_code::OK) {
            assert(intf);
            if (intf) {
                intf->unrefNoDelete(); // balance queryInterface()
                return true;
            }
        }
        return false;
    }

    template <typename T>
    T* cast()
    {
        T* intf;
        if (this->queryInterface(IID(T), (void**)&intf) != xp_error_code::OK) {
            return nullptr;
        }
        intf->unrefNoDelete(); // Balance counter (incremented within queryInterface)
        return intf;
    }
};

template <typename T, typename F>
constexpr T* intf_cast(gsl::not_null<F> from)
{
    T* intf;
    if (from->queryInterface(IID(T), (void**)&intf) != xp_error_code::OK) {
        return nullptr;
    }
    intf->unrefNoDelete(); // Balance counter (incremented within queryInterface)
    return intf;
}


#define IID_IINTERFACE IID(IInterface)


struct IQueryState {
    virtual void addSearched(void*) = 0;
    virtual bool isSearched(void*) const = 0;

    virtual ~IQueryState() = default;
};

struct IBus;

/**
 * \interface IInterfaceEx
 * \brief root of all bus-aware extensible interfaces
 */
struct IInterfaceEx : public IInterface {
    DECLARE_IID("632B176F-E7B9-4557-9657-15DB3AC94FBC");
    /**
     *	Interface browsing, returns 0 if successful, non-zero error code if fails.
     */
    virtual xp_error_code queryInterfaceEx(TIntfId iid, void** retIntf, IQueryState& qst) = 0;
    /**
     * set the hosting bus
     */
    virtual void setBus(IBus* bus) = 0;

    /**
     * [EXPLICIT-RELEASE]
     *
     * for a bus:
     * all connected interfaces (and buses) will not be called after this api.
     * the internal resources can be released safely at this point.
     *
     * for a normal service:
     * any apis should be disabled after this call, and internal resouces can be released safely
     * at this point
     */
    virtual void finish() = 0;
};

#define IID_IINTERFACEEX IID(IInterfaceEx)

/**
 * @brief Try resolving an interface from an interface extension.
 *
 * @param pex interface extension being queried
 * @param iid unique id of interface to resolve
 * @param retIntf [out] the interface resolved.
 * @param qst current query helper
 * @return xp_error_code
 */
inline xp_error_code resolve(gsl::not_null<IInterfaceEx*> pex, TIntfId iid, void** retIntf, IQueryState& qst)
{
    if (!qst.isSearched(pex)) {
        return pex->queryInterfaceEx(iid, retIntf, qst);
    }
    return xp_error_code::INTF_NOT_RESOLVED;
}

/**
 * \interface IBus
 * \brief Interface integration bus is used to connect multiple interfaces on the fly.
 *
 * Interface Bus itself can be inter-connected to form a more complex topology.
 *
 * Note: only IInterfaceEx-derived interfaces can be connected via IBus.
 * However, it is possible that other more generic IInterface or Windows IUnknown can be supported
 * as well by interface hooking.
 */
struct IBus : public IInterfaceEx {
    DECLARE_IID("B7914714-4159-48C6-BFF3-A21C6F0BB1CA");

    /**
     * @brief Connect the intf to this bus.
     *
     * @param intf can be a normal interface or a IBus. For a IBus, only the bus with higher or equal bus level
     * can be successfully connected to this bus for security concern, because by design the low level bus should
     * hosting more secure interfaces which should not be browsed from less secure interfaces on higher level bus.
     *
     *  connect(interface _or_ less-secure-bus)
     *
     * @param order When the interface should be finished? (applied to non-bus interface only)
     *      the most basic interface should have a higher pass value (service closed later).
     *
     *
     * @return true if the interface is connected successfully
     */
    [[nodiscard]] virtual bool connect(gsl::not_null<IInterfaceEx*> intf, int order = 0) = 0;

    /**
     * Disconnect the intf ( a normal interface or an interace bus) from this bus.
     * After disconnection, the intf itself and all interfaces hosted on it (for a bus) cannot be reached from
     * interface browsing, however, if it is already retrieved and locked before the disconnection, the interface
     * can still be used until it is released.
     */
    virtual void disconnect(gsl::not_null<IInterfaceEx*> intf) = 0;
    /**
     * Get the Bus Level
     *
     * bus level 0 is the top secret level.
     */
    virtual int level() const = 0;
    /**
     * Find a bus with specified bus level.
     *
     * Note there might be multiple buses with the same level in a complex bus network, and the programmer
     * should know what he is doing.
     */
    virtual IBus* findFirstBusByLevel(int busLevel) const = 0;

    /**
     *  add a sibling bus as a weak reference.
     */
    virtual void addSiblingBus(gsl::not_null<IBus*> bus) = 0;
    /**
     * Remove weak reference to a sibling bus.
     * called when the sibling bus is being destroyed.
     */
    virtual void removeSiblingBus(gsl::not_null<IBus*> bus) = 0;
};

#define IID_IBUS IID(IBus)

/**
 * \class IEnumerator
 * \brief Generic value enumerator
 */
template <typename T>
struct IEnumerator : public IRefObj {
    /// Is next value available?
    virtual bool hasNext() = 0;
    /// The next values
    virtual T next() = 0;
};

/**
 * \class IEnumeratorEx
 * \brief Enhanced Generic value enumerator (with new \e size() )
 */
template <typename T>
struct [[nodiscard]] IEnumeratorEx : public IRefObj {
    /// Is next value available?
    virtual bool hasNext() = 0;
    /// The next value
    virtual gsl::not_null<T> next() = 0;
    /// Get the total number of values.
    virtual std::size_t size() const = 0;
    /// Random access by index.
    virtual gsl::not_null<T> get(unsigned int index) const = 0;
    // go to first element to re-start the enumeration
    virtual void rewind() = 0;
};

//----- Helper ------
/**
 * \class auto_ref
 * \brief Helper class for automatic reference counting.
 *
 * Usage:
 *
 * \code
 *
 * auto_ref<ISecureBox> box (new Impl_SecureBox());
 *
 *
 * \endcode
 */
template <class T>
class auto_ref
{
public:
    auto_ref() = default;

    auto_ref(auto_ref& rv) : _intf(rv._intf)
    {
        if (_intf)
            _intf->ref();
    }
    auto_ref(auto_ref&& rv) : _intf(rv._intf)
    {
        rv._intf = nullptr;
    }

    template <typename U>
    auto_ref(U from) : _intf{nullptr}
    {
        if constexpr (std::is_convertible_v<U, decltype(_intf)>) {
            _intf = from;
            if (_intf)
                _intf->ref();
        } else {
            from->queryInterface(IID(T), (void**)&_intf);
        }
    }

    auto_ref(T* intf, bool refIt = true) : _intf(intf)
    {
        if (refIt && _intf)
            _intf->ref();
    }

    ~auto_ref()
    {
        clear();
    }

    template <typename U>
    void operator=(U intf)
    {
        if constexpr (std::is_same_v<nullptr_t, U>) {
            clear(); // support = nullptr
        } else if constexpr (std::is_same_v<decltype(NULL), U>) {
            clear(); // support = NULL
        } else {
            using V = std::remove_pointer_t<U>;
            if constexpr (std::is_same_v<T, V> || std::is_base_of_v<T, V>) {
                if (_intf != intf) {
                    if (_intf)
                        _intf->unref();
                    _intf = intf;
                    if (_intf)
                        _intf->ref();
                }
            } else {
                intf->queryInterface(IID(T), (void**)&_intf);
            }
        }
    }

    auto_ref& operator=(const auto_ref& intf)
    {
        if (_intf != intf.get()) {
            if (_intf)
                _intf->unref();
            _intf = intf.get();
            if (_intf)
                _intf->ref();
        }
        return *this;
    }
    auto_ref& operator=(auto_ref&& rref)
    {
        if (_intf != rref._intf) {
            if (_intf)
                _intf->unref();
            _intf = rref._intf;
            rref._intf = nullptr;
        }
        return *this;
    }

    inline T& operator*() const
    {
        assert(_intf);
        return *_intf;
    }

    // Return managed pointer and release my ownership
    T* release()
    {
        T* p = _intf;
        _intf = nullptr;
        if (p) {
            p->unrefNoDelete(); // remove my reference without destroying the resouce.
        }
        return p;
    }

    inline operator T*(void) const
    {
        return _intf;
    }
    // The caller just reference it and will *not* unref() the pointer when not using it anymore.
    inline T* get() const
    {
        return _intf;
    }
    /**
     * The caller should call \e unref() to release the pointer when not using it anymore.
     *
     * \code
     * 		auto_ref<INode> _node;
     * 		INode* QueryInterface(IID id){
     * 			if(id == IID(INode))
     * 				return _node.getRef();
     * 		}
     * \endcode
     */

    inline T* getRef()
    {
        if (_intf) {
            _intf->ref();
        }
        return _intf;
    }
    inline T* operator->() const
    {
        return _intf;
    }

    inline operator bool() const
    {
        return _intf != nullptr;
    }

    void clear()
    {
        if (_intf) {
            _intf->unref();
            _intf = nullptr;
        }
    }

private:
    T* _intf{nullptr};

    NO_HEAP;
};

// deduction guide to support:
//
//   auto_ref var = new T();
//
template <class T>
auto_ref(T*) -> auto_ref<T>;

template <typename T>
struct checked_unref {
    typedef void result_type;
    typedef T* argument_type;

    void operator()(T* p) const
    {
        if (p) {
            p->unref();
        }
    }
};

template <typename T>
struct checked_ref {
    typedef void result_type;
    typedef T* argument_type;

    void operator()(T* p) const
    {
        if (p) {
            p->ref();
        }
    }
};

// Connect intr face only if it is not plugged in.
#define BUS_CONNECT_INTERFACE(bus, intf, inst)                              \
    {                                                                       \
        if (!bus->connect(inst))                                            \
            XP_TRACE_WARN("interface [%s] cannot be connected", intf::iid); \
    }

} // namespace xp

#endif
