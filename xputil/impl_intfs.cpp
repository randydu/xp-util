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
bool equalIID(const TIntfId id1, const TIntfId id2)
{
    return 0 == strcmp(id1, id2);
}

namespace {
std::logic_error api_already_disabled("api already disabled!");
}

// TBus
void TBus::assert_api_not_closed() const
{
    if (_status == CLEARED)
        throw api_already_disabled;
}

void TBus::clear()
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
        for (std::vector<IInterfaceEx*>::reverse_iterator it = _intfs.rbegin(); it != _intfs.rend(); ++it) {
            IInterfaceEx* intf = *it;
            if (pass == intf->getFinishPass()) {
                intf->finish();
            }
        }
    }
    for (auto intf : _intfs) {
        intf->_setBus(nullptr);
        intf->unref();
    }
    _intfs.clear();

    for (std::vector<IBus*>::reverse_iterator it = _buses.rbegin(); it != _buses.rend(); ++it) {
        IBus* bus = *it;
        bus->finish();
        bus->_setBus(nullptr);
        bus->unref();
    }
    _buses.clear();

    _status = CLEARED;
}

bool TBus::connect(IInterfaceEx* intf)
{
    assert_api_not_closed();

    if (intf == nullptr || intf == this)
        return false; // no loop-back.

    IBus* bus;
    TQueryState qst;
    if (0 == intf->_queryInterface(IID_IBUS, (void**)&bus, qst)) {
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
            _siblings.push_back(bus);
            // sibling bus, mutual connection
            bus->addSiblingBus(this);

            return true;
        }

        // bus level smaller than mine, connection failure.
        return false;
    }

    // no duplicated interfaces
    if (auto it = std::find(_intfs.begin(), _intfs.end(), intf); it != _intfs.end())
        return false;

    intf->ref();
    _intfs.push_back(intf);
    intf->_setBus(this);
    return true;
}

void TBus::disconnect(IInterfaceEx* intf)
{
    assert_api_not_closed();

    // interfaces first
    if (auto it = find(_intfs.begin(), _intfs.end(), intf); it != _intfs.end()) {
        _intfs.erase(it);
        intf->_setBus(nullptr);
        intf->unref();
        return;
    }
    // buses later
    if (auto it = find(_buses.begin(), _buses.end(), intf); it != _buses.end()) {
        _buses.erase(it);
        intf->unref();
        return;
    }

    removeSiblingBus(static_cast<IBus*>(intf));
}

IBus* TBus::findFirstBusByLevel(int busLevel) const
{
    assert_api_not_closed();

    if (busLevel < _level)
        return nullptr;

    if (_level == busLevel)
        return (IBus*)this;

    for (auto bus : _buses) {
        auto p = bus->findFirstBusByLevel(busLevel);
        if (p)
            return p;
    }

    for (auto bus : _siblings) {
        auto p = bus->findFirstBusByLevel(busLevel);
        if (p)
            return p;
    }

    return nullptr;
}

void TBus::addSiblingBus(IBus* bus)
{
    assert_api_not_closed();

    if(auto it = find(_siblings.begin(), _siblings.end(), bus); it == _siblings.end()) {
        _siblings.push_back(bus);
    }
}

void TBus::removeSiblingBus(IBus* bus)
{
    assert_api_not_closed();

    if(auto it = find(_siblings.begin(), _siblings.end(), bus); it != _siblings.end()) {
        _siblings.erase(it);
    }
}

int TBus::_queryInterface(TIntfId iid, void** retIntf, IQueryState& qst)
{
    if (equalIID(iid, IID_IBUS) || equalIID(iid, IID_IINTERFACEEX) || equalIID(iid, IID_IINTERFACE)) {
        *retIntf = (IInterface*)(this);
        this->ref();
        return 0;
    }

    assert_api_not_closed();

    qst.addSearched(this);

    // scanning interfaces in my slots
    for (auto intf : _intfs) {
        if (!qst.isSearched(intf) && intf->_queryInterface(iid, retIntf, qst) == 0) {
            return 0;
        }
    }
    // scan sibling buses
    for (auto bus : _siblings) {
        if (!qst.isSearched(bus) && bus->_queryInterface(iid, retIntf, qst) == 0) {
            return 0;
        }
    }
    // scanning connected upper-level/less-secure buses
    for (auto bus : _buses) {
        if (!qst.isSearched(bus) && bus->_queryInterface(iid, retIntf, qst) == 0) {
            return 0;
        }
    }

    return 1;
}

} // namespace xp
