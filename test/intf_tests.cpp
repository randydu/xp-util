#include "catch_macros.h"

#include <xputil/impl_intfs.h>

namespace {
constexpr auto tag = "[intf]";

struct Dummy : public xp::IRefObj {
    Dummy() { count++; }
    ~Dummy() { count--; }
    static int count;
};
int Dummy::count = 0;

struct IDummy : public xp::IInterface {
    DECLARE_IID("dummy.2020");

    IDummy()
    {
        count++;
    }
    ~IDummy()
    {
        count--;
    }

    int value() const
    {
        return 1;
    }
    static int count;
};
int IDummy::count = 0;
struct IBaz : public xp::IInterfaceEx {
    DECLARE_IID("aec95632-777d-4bda-9e14-d93f2a77677e");
    std::string id() const { return "baz"; }

    IBaz() { count++; }
    virtual ~IBaz() { count--; }
    static int count;
};
int IBaz::count = 0;

struct IFoo : public xp::IInterfaceEx {
    DECLARE_IID("23c88882-8edb-4b04-a017-e2be0b68acea");
    virtual int foo() const = 0;
    virtual std::string id() const = 0;
};

struct Foo : IFoo {
    virtual int foo() const override { return 1; };
    virtual std::string id() const override { return "foo"; }

    Foo() { count++; }
    virtual ~Foo() { count--; }
    static int count;
};
int Foo::count = 0;

struct IBar : public xp::IInterfaceEx {
    DECLARE_IID("e1205e5b-ecb2-436b-91e9-6fcd5a9631d2");
    virtual int bar() const = 0;
    virtual std::string id() const = 0;
};

struct Bar : IBar {
    virtual int bar() const override { return 2; };
    virtual std::string id() const override { return "bar"; }

    Bar() { count++; }
    virtual ~Bar() { count--; }
    static int count;
};
int Bar::count{0};


struct Foobar : virtual IFoo, virtual IBar {
    virtual int foo() const override { return 3; }
    virtual int bar() const override { return 4; }
    virtual std::string id() const override { return "foobar"; }

    Foobar() { count++; }
    virtual ~Foobar() { count--; }
    static int count;
};
int Foobar::count{0};

} // namespace

TEST_CASE("refobj", tag)
{
    SECTION("manual ref")
    {
        auto p = new xp::TRefObj<Dummy>;
        CHECK(p->count() == 0);

        SECTION("usage")
        {
            // lock the reference before accessing
            p->ref();
            CHECK(p->count() == 1);

            //... do whatever we want

            // must unlock the reference before leaving
            p->unref();

            // p is now a dangling pointer!!!
        }
    }

    SECTION("auto ref")
    {
        xp::auto_ref p = new xp::TRefObj<Dummy>;
        CHECK(p->count() == 1);
    }

    CHECK(Dummy::count == 0);
}

TEST_CASE("interface", tag)
{
    using namespace xp;

    CHECK(IDummy::count == 0);
    CHECK(Foo::count == 0);

    SECTION("cast")
    {
        auto_ref obj = new TInterface<IDummy>();

        CHECK(IID(IDummy) == std::string("dummy.2020"));
        CHECK(obj->cast<IInterface>() == obj.get());
        CHECK(obj->cast<IDummy>() == obj.get());

        CHECK(obj->supports(IID(IDummy)));
        CHECK(obj->supports(IID(IInterface)));
        CHECK_FALSE(obj->supports(IID(IInterfaceEx)));
        CHECK_FALSE(obj->supports(IID(IFoo)));

        auto_ref dummy = obj->cast<IDummy>();
        CHECK(dummy->value() == 1);
    }

    CHECK(IDummy::count == 0);
    CHECK(Foo::count == 0);
}

TEST_CASE("interface-ex", tag)
{
    using namespace xp;

    CHECK(Foo::count == 0);
    CHECK(Bar::count == 0);

    SECTION("foo")
    {
        auto_ref obj = new TInterfaceEx<Foo>();

        CHECK(obj->supports(IID(IFoo)));
        CHECK(obj->supports(IID(IInterface)));
        CHECK(obj->supports(IID(IInterfaceEx)));
        CHECK_FALSE(obj->supports(IID(IBar)));

        auto_ref foo = obj->cast<IFoo>();
        CHECK(foo->id() == "foo");
    }

    CHECK(Foo::count == 0);
    CHECK(Bar::count == 0);
}

class IHello : public xp::IInterface
{
    virtual void hello() = 0;
};

class Impl_Hello : public IHello
{
public:
    Impl_Hello(const std::string& name, int age) : name_(name), age_(age) {}
    Impl_Hello(std::string&& name, int age) : name_(std::move(name)), age_(age) {}

    virtual void hello() {}

    std::string name_;
    int age_{0};
};

class Impl2_Hello : public xp::TInterface<IHello>
{
public:
    Impl2_Hello() {}
    Impl2_Hello(const std::string& name, int age) : name_(name), age_(age) {}
    Impl2_Hello(std::string&& name, int age) : name_(std::move(name)), age_(age) {}

    virtual void hello() {}

    std::string name_;
    int age_{0};
};
using namespace std::string_literals;
const auto NAME = "Randy"s;

TEST_CASE("perfect type forward", tag)
{
    SECTION("cref")
    {
        xp::auto_ref mi = new xp::TInterface<Impl_Hello>(NAME, 100);
        CHECK(mi->name_ == NAME);
        CHECK(mi->age_ == 100);
    }
    SECTION("rref")
    {
        xp::auto_ref mi = new xp::TInterface<Impl_Hello>("Randy"s, 100);
        CHECK(mi->name_ == "Randy"s);
        CHECK(mi->age_ == 100);
    }
}

TEST_CASE("make_xxx", tag)
{
    {
        xp::auto_ref mi = xp::make_intf<Impl_Hello>(NAME, 100);
        CHECK(mi->name_ == NAME);
        CHECK(mi->age_ == 100);
    }
    {
        xp::auto_ref mi = xp::make_intf<Impl2_Hello>("Randy"s, 100);
        CHECK(mi->name_ == NAME);
        CHECK(mi->age_ == 100);
    }
}

TEST_CASE("intf-auto-browse", tag)
{
    CHECK(Foo::count == 0);
    SECTION("different auto-ref def")
    {
        using namespace xp;
        auto_ref bus1 = new TBus(1);
        auto_ref p1 = new TInterfaceEx<Foo>();
        CHECK(bus1->connect(p1));

        auto_ref my_foo_1 = bus1->cast<IFoo>();
        auto_ref my_foo_1_{bus1->cast<IFoo>()};

        auto_ref<IFoo> my_foo_3(new TInterfaceEx<Foo>());
        auto_ref my_foo_3_(new TInterfaceEx<Foo>());
        auto_ref<IFoo> my_foo_4{new TInterfaceEx<Foo>()};
        auto_ref my_foo_4_{new TInterfaceEx<Foo>()};

        auto_ref my_foo_(bus1);
        auto_ref<IFoo> my_foo(bus1);
        CHECK(my_foo.get() == p1.get());

        auto_ref<IFoo> p2 = my_foo;
        auto_ref p2_ = my_foo;
        auto_ref p3{my_foo};

        auto_ref<IFoo> my_bus = bus1;
    }
    CHECK(Foo::count == 0);
}

TEST_CASE("bus", tag)
{
    using namespace xp;

    CHECK(Foo::count == 0);
    CHECK(Bar::count == 0);
    CHECK(IBaz::count == 0);

    SECTION("single bus")
    {
        auto_ref bus = new TBus(0);
        CHECK(bus->count() == 1);
        CHECK(bus->level() == 0);
        CHECK_FALSE(bus->finished());

        SECTION("same interface cannot be added to the same bus")
        {
            auto_ref p1 = new TInterfaceEx<Foo>();
            CHECK(bus->connect(p1));
            CHECK_FALSE(bus->connect(p1));
        }

        SECTION("same interface cannot be added to different buses")
        {
            auto_ref bus1 = new TBus(1);
            auto_ref p1 = new TInterfaceEx<Foo>();

            CHECK(bus->connect(p1));
            CHECK_FALSE(bus1->connect(p1));
        }

        SECTION("no bus loopback")
        {
            CHECK_FALSE(bus->connect(bus));
        }

        SECTION("valid augument")
        {
            CHECK_FALSE(bus->connect(nullptr));
        }

        SECTION("disconnect")
        {
            SECTION("single interface")
            {
                auto_ref p1 = new TInterfaceEx<Foo>();
                CHECK(bus->connect(p1));
                CHECK(bus->total_intfs() == 1);

                bus->disconnect(p1);

                CHECK(bus->total_intfs() == 0);
            }
            SECTION("disconnect a bus")
            {
                auto_ref bus1 = new TBus(1);
                CHECK(bus1->connect(new TInterfaceEx<Foo>()));
                CHECK(bus->connect(bus1));

                CHECK(bus->total_buses() == 1);
                bus->disconnect(bus1);

                CHECK(bus->total_buses() == 0);
            }
        }

        SECTION("single bus with two interfaces")
        {
            CHECK(bus->connect(new TInterfaceEx<Foo>()));
            CHECK(bus->connect(new TInterfaceEx<Bar>()));

            SECTION("bus => IFoo")
            {
                auto_ref foo = bus->cast<IFoo>();
                CHECK(foo);
                CHECK(foo->id() == "foo");

                SECTION("IFoo => IBar")
                {
                    auto_ref bar = foo->cast<Bar>();
                    CHECK(bar);
                    CHECK(bar->id() == "bar");

                    SECTION("IBar => IBus")
                    {
                        auto_ref my_bus = bar->cast<IBus>();
                        CHECK(my_bus.get() == bus.get());
                    }
                }
            }

            SECTION("bus => IBar")
            {
                auto_ref bar = bus->cast<IBar>();
                CHECK(bar);
                CHECK(bar->id() == "bar");

                SECTION("IBar => IFoo")
                {
                    auto_ref foo = bar->cast<IFoo>();
                    CHECK(foo);
                    CHECK(foo->id() == "foo");

                    SECTION("IFoo => IBus")
                    {
                        auto_ref my_bus = foo->cast<IBus>();
                        CHECK(my_bus.get() == bus.get());
                    }
                }
            }

            CHECK(bus->count() == 1);
            CHECK(bus->total_intfs() == 2);
            CHECK(bus->total_buses() == 0);

            SECTION("bus finish")
            {
                bus->finish();
                CHECK(bus->finished());
                CHECK(bus->count() == 1);
                CHECK(bus->total_intfs() == 0);
                CHECK(bus->total_buses() == 0);
            }
        }
    }

    SECTION("two buses")
    {
        auto_ref bus0 = new TBus(0);
        auto_ref foo = new TInterfaceEx<Foo>();
        CHECK(bus0->connect(foo));

        CHECK(bus0->total_intfs() == 1);
        CHECK(bus0->total_buses() == 0);

        SECTION("bus-1")
        {
            auto_ref bus1 = new TBus(1);
            auto_ref bar = new TInterfaceEx<Bar>();
            CHECK(bus1->connect(bar));

            CHECK_FALSE(bus1->connect(bus0));
            CHECK(bus0->connect(bus1));

            CHECK(bus0->total_buses() == 1);
            CHECK(bus1->total_buses() == 0);

            SECTION("bus0::foo => bus1::bar")
            {
                auto_ref bar = foo->cast<IBar>();
                CHECK(bar);
                CHECK(bar.get() == bar.get());
            }
            SECTION("bus1::bar !=> bus0::foo")
            {
                auto_ref foo = bar->cast<IFoo>();
                CHECK_FALSE(foo);
            }

            CHECK(bus1->count() == 2); // bus0-connected + local auto_ref
        }

        SECTION("sibling bus [0,0]")
        {
            auto_ref bus1 = new TBus(0);
            auto_ref bar = new TInterfaceEx<Bar>();
            CHECK(bus1->connect(bar));

            CHECK(bus1->connect(bus0));

            CHECK(bus0->total_buses() == 0);
            CHECK(bus0->total_siblings() == 1);
            CHECK(bus1->total_buses() == 0);
            CHECK(bus1->total_siblings() == 1);

            SECTION("bus0::foo => bus1::bar")
            {
                auto_ref bar = foo->cast<IBar>();
                CHECK(bar);
                CHECK(bar.get() == bar.get());
            }
            SECTION("bus1::bar => bus0::foo")
            {
                auto_ref foo = bar->cast<IFoo>();
                CHECK(foo);
            }

            CHECK(bus1->count() == 1); // local auto_ref

            bus1->finish();
            CHECK(bus1->finished());
            CHECK_FALSE(bus0->finished()); // bus0 not affected.
        }

        SECTION("sibling bus, no dangling bus")
        {
            CHECK_FALSE(bus0->connect(new TBus(0)));
            CHECK(bus0->connect(new TBus(1)));
        }

        CHECK(bus0->count() == 1);
    }

    SECTION("three buses")
    {
        using namespace xp;

        SECTION("cascade [0->1->2]")
        {
            auto_ref bus0 = new TBus(0);
            CHECK(bus0->connect(new TInterfaceEx<IBaz>()));
            auto baz = bus0->cast<IBaz>();
            CHECK(baz != nullptr);

            auto_ref bus1 = new TBus(1);
            CHECK(bus1->connect(new TInterfaceEx<Foo>()));
            auto foo = bus1->cast<IFoo>();
            CHECK(foo != nullptr);

            auto_ref bus2 = new TBus(2);
            CHECK(bus2->connect(new TInterfaceEx<Bar>()));
            auto bar = bus2->cast<IBar>();
            CHECK(bar != nullptr);

            CHECK(bus0->connect(bus1));
            CHECK(bus0->total_intfs() == 1);
            CHECK(bus0->total_buses() == 1);
            CHECK(bus0->total_siblings() == 0);

            CHECK(bus1->connect(bus2));
            CHECK(bus1->total_intfs() == 1);
            CHECK(bus1->total_buses() == 1);
            CHECK(bus1->total_siblings() == 0);

            CHECK(bus2->total_intfs() == 1);
            CHECK(bus2->total_buses() == 0);
            CHECK(bus2->total_siblings() == 0);

            // upward cast
            CHECK(baz->cast<IFoo>() == foo);
            CHECK(baz->cast<IBar>() == bar);
            CHECK(foo->cast<IBar>() == bar);

            // downward cast
            CHECK(bar->cast<IFoo>() == nullptr);
            CHECK(bar->cast<IBaz>() == nullptr);
            CHECK(foo->cast<IBaz>() == nullptr);

            // bus-cast
            CHECK(baz->cast<IBus>() == bus0);
            CHECK(foo->cast<IBus>() == bus1);
            CHECK(bar->cast<IBus>() == bus2);

            CHECK(bus0->findFirstBusByLevel(0) == bus0.get());
            CHECK(bus0->findFirstBusByLevel(1) == bus1.get());
            CHECK(bus0->findFirstBusByLevel(2) == bus2.get());
            CHECK(bus0->findFirstBusByLevel(3) == nullptr);

            CHECK(bus1->findFirstBusByLevel(0) == nullptr);
            CHECK(bus1->findFirstBusByLevel(1) == bus1.get());
            CHECK(bus1->findFirstBusByLevel(2) == bus2.get());
            CHECK(bus1->findFirstBusByLevel(3) == nullptr);

            CHECK(bus2->findFirstBusByLevel(1) == nullptr);
            CHECK(bus2->findFirstBusByLevel(2) == bus2.get());
            CHECK(bus2->findFirstBusByLevel(3) == nullptr);
        }

        SECTION("[0, 0 -> 1]")
        {
            auto_ref bus0 = new TBus(0);
            auto_ref bus01 = new TBus(0);

            auto_ref bus1 = new TBus(1);

            CHECK(bus01->connect(bus1));
            CHECK(bus0->connect(bus01));

            CHECK(bus0->total_intfs() == 0);
            CHECK(bus0->total_siblings() == 1);
            CHECK(bus0->total_buses() == 0);

            CHECK(bus01->total_intfs() == 0);
            CHECK(bus01->total_siblings() == 1);
            CHECK(bus01->total_buses() == 1);

            CHECK(bus1->total_intfs() == 0);
            CHECK(bus1->total_siblings() == 0);
            CHECK(bus1->total_buses() == 0);

            CHECK(bus0->findFirstBusByLevel(0) == bus0.get());
            CHECK(bus0->findFirstBusByLevel(1) == bus1.get());

            CHECK(bus01->findFirstBusByLevel(0) == bus01.get());
            CHECK(bus01->findFirstBusByLevel(1) == bus1.get());

            CHECK(bus1->findFirstBusByLevel(0) == nullptr);
            CHECK(bus1->findFirstBusByLevel(1) == bus1.get());
        }
    }

    CHECK(IBaz::count == 0);
    CHECK(Foo::count == 0);
    CHECK(Bar::count == 0);
}

TEST_CASE("ref-issue", tag)
{
    using namespace xp;

    SECTION("bus nav")
    {
        auto_ref<IBus> bus = new TBus(0);
        bus->connect(new TInterfaceEx<Foo>());

        {
            IFoo* ifoo = bus->cast<IFoo>();
            CHECK(static_cast<IRefObj*>(ifoo)->count() == 1); // internal refed by bus.
        }

        { // copy constructor
            auto_ref<IFoo> foo = bus;
            CHECK(static_cast<IRefObj*>(foo.get())->count() == 2); // refered by bus and foo.
        }

        // assignment constructor
        {
            auto_ref<IFoo> foo;
            foo = bus;
            CHECK(static_cast<IRefObj*>(foo.get())->count() == 2); // should be refered by bus and foo.
        }
    }
    SECTION("assignment auto_ref => auto_ref")
    {
        {
            auto_ref<IFoo> foo;
            auto_ref<IFoo> bar(new xp::TInterfaceEx<Foo>());
            foo = bar;
            CHECK(static_cast<IRefObj*>(foo.get())->count() == 2); // should be refered by foo and bar.
            CHECK(Foo::count == 1);                                // only one shared instance
        }
        {
            auto_ref<IFoo> foo;
            auto_ref<IFoo> bar(new xp::TInterfaceEx<Foo>());
            foo = std::move(bar);
            CHECK(static_cast<IRefObj*>(foo.get())->count() == 1); // only be refered by foo.
            CHECK(Foo::count == 1);                                // only one shared instance
        }
    }
}

TEST_CASE("multi-intfx", tag)
{
    xp::auto_ref fb = new xp::TMultiInterfaceEx<Foobar, IFoo, IBar>();
    CHECK(Foobar::count == 1);
    CHECK(fb->count() == 1);

    CHECK(fb->id() == "foobar");
    CHECK(fb->foo() == 3);
    CHECK(fb->bar() == 4);

    SECTION("IFoo")
    {
        xp::auto_ref<IFoo> foo(fb);
        CHECK(foo);
        CHECK(foo->id() == "foobar");
        CHECK(foo->foo() == 3);
        CHECK(foo->count() == 2);
    }
    SECTION("IBar")
    {
        xp::auto_ref<IBar> bar(fb);
        CHECK(bar);
        CHECK(bar->id() == "foobar");
        CHECK(bar->bar() == 4);
        CHECK(bar->count() == 2);
    }
}