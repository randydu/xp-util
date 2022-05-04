/*
 * Impl_Intfs.cpp
 *
 *  Created on: Feb 19, 2010
 *      Author: dujie
 */

#include "impl_intfs.h"

#include <stdexcept>
#include <string.h>

namespace {
constexpr int max_clear_pass = 3;
}

namespace xp {

namespace {
std::logic_error api_already_disabled("api already disabled!");
}

xp_error_code resolve(gsl::not_null<IInterfaceEx*> pex, TIntfId iid, void** retIntf, IQueryState& qst)
{
    if (!qst.isSearched(pex)) {
        return pex->queryInterfaceEx(iid, retIntf, qst);
    }
    return xp_error_code::INTF_NOT_RESOLVED;
}
// TBus
void TBus::assert_api_not_closed() const
{
    if (_status == CLEARED)
        throw api_already_disabled;
}

void TBus::reset()
{
    if (_status != ACTIVE)
        return;

    _status = CLEARING;

    for (auto p : _siblings) {
        p->removeSiblingBus(this);
        // sibling bus not affected by my clearance.
        // p->finish();
    }
    _siblings.clear();

    // explicitly pass-ordered resource release
    // for the same pass, the later installed interface is released first.
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

    _status = CLEARED;
}

bool TBus::connect(gsl::not_null<IInterfaceEx*> intf, int order)
{
    assert_api_not_closed();

    if (intf == this) return false; // no loop-back.


    IBus* bus;
    detail::QueryState qst;
    if (intf->queryInterfaceEx(IID_IBUS, (void**)&bus, qst) == xp_error_code::OK) {
        ON_EXIT(bus->unref();); // balance queryInterface

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

void TBus::disconnect(gsl::not_null<IInterfaceEx*> intf)
{
    assert_api_not_closed();

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

    removeSiblingBus(static_cast<IBus*>(intf.get()));
}

IBus* TBus::findFirstBusByLevel(int busLevel) const
{
    assert_api_not_closed();

    if (busLevel < _level)
        return nullptr;

    if (_level == busLevel)
        return (IBus*)this;

    for (auto bus : _buses) {
        if (auto p = bus->findFirstBusByLevel(busLevel); p) return p;
    }

    for (auto bus : _siblings) {
        if(auto p = bus->findFirstBusByLevel(busLevel);p) return p;
    }

    return nullptr;
}

void TBus::addSiblingBus(gsl::not_null<IBus*> bus)
{
    assert_api_not_closed();

    _siblings.insert(bus);
}

void TBus::removeSiblingBus(gsl::not_null<IBus*> bus)
{
    assert_api_not_closed();

    _siblings.erase(bus);
}

xp_error_code TBus::queryInterfaceEx(TIntfId iid, void** retIntf, IQueryState& qst)
{
    Expects(retIntf);
    *retIntf = nullptr;

    if (equalIID(iid, IID_IBUS) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
        *retIntf = (IInterfaceEx*)(this);
        this->ref();
        return xp_error_code::OK;
    }

    assert_api_not_closed();

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

} // namespace xp
