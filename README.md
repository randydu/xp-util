# Cross-Platform Utility Library (XP-UTIL)

This repository shares some c++ utility classes used in my own projects but can also be useful for other people to develop c++ applications on Windows, Linux and Mac.

## Compatibility

C++17

## Install


## Usage



### OnExit: RAII Implementation

__"Resource acquisition is initialization"__ (RAII) is great for C++ to keep control the lifetime of a resource in the code manually. In standard library we have many built-in classes (such as std::unique_ptr<>, std::lock_guard, etc.) to help c++ programmers to manage shared resources. However, we might have to develop a RAII wrapper class for non-standard resource types, which is too tedious, boring and error-prone, this repeated micro-design efforts can be completely eliminated by a template-macro based facility demonstrated as following:

```c++
#include <xputil/on_exit.h>

Connection_t* conn = create_connection(...); //create a new connection to database
ON_EXIT(destroy_connection(conn));

// uses the connection
get_data(conn, params,...);

```

Another example if multi-stage-object-initialization. 

Assuming a complex c++ object with multiple resources will be created, we only want to return a valid instance to our api caller with all needed resources properly initialized. No resource leakage is allowed during the phrase of resources initialization.

The example is demonstrated by a _Car_ instance with one _Engine_ and several _Wheels_.  In the source code, we demonstrate how to create a new class instance with multiple internal resources allocated step by step. 

For each step, before allocating a new resource, an ON_EXIT() clause is declared to make sure the resource currently allocated can be released (revoked) properly if any unexpected error occurs to any _later_ steps. 

```c++
class EngineFactory {
public:
    static Engine* get(); //get a new engine, might throw if engine out-of-stock
    static void revoke(Engine* engine); //revoke an old engine for recycle
};

class WheelFactory {
public:
    static Wheel* get(); //get a new wheel, might throw if wheel out-of-stock
    static void revoke(Wheel* wheel); //revoke an old wheel for recycle
};

class Car {
public:
    //Returns a car instance with all resources properly initialized and self-test passed.
    static std::unique_ptr<Car> create(int total_wheels = 4){
        bool success {false};
        Car* car {nullptr};

        //step 0: allocate a "raw" car instance with no resouces allocated.
        ON_EXIT(
            if(!success && car) delete car;
        )
        Car* car = new Car();

        //step 2: allocate one engine for the car instance
        ON_EXIT(
            if(!success && car->engine_){ EngineFactory::revoke(car->engine_); }
        )
        car->engine_ = EngineFactory::get();

        //step 3: allocate all wheels
        ON_EXIT(
            if(!success){
                for(auto p: wheels_){
                    if(p) WheelFactory::revoke(p);
                }
            }
        );
        car->wheels.resize(total_wheels);
        for(int i = 0; i < total_wheels; ++i){
            car->wheels[i] = EngineFactory::get();
        }

        //Now that all resources are allocated successfully,
        //raise the SUCCESS flag only if the self-testing works fine.
        success = car->is_valid();
        return car;
    }
    ~Car(){
        EngineFactory::revoke(engine);
        for(auto p: wheels) EngineFactory::revoke(p);
    }

private:
    Car() = default; //initialize all resource pointers to nullptr by default.

    bool self_test_passed() const {
        ... //self-test logics
        return true;
    }

    bool is_valid() const {
        return engine  //engine is ok
            && std::all_of(std::begin(wheels), std::end(wheels), [](wheel* p){ return p != nullptr; }) //all wheels are ok
            && self_test_passed(); //all functions passed self-test.
    }

    //Resources
    Engine* engine_;
    std::vector<Wheel*> wheels_;
};
```

Without the ON_EXIT() facility, we might have to manually release the resources already allocated in the previous steps:

```c++

ResourceA *ra = allocate_resource_a();
if(ra == nullptr) throw std::runtime_error("Resource-A allocation error");

ResourceB *rb = allocate_resource_b();
if(rb == nullptr){
    free_resource_a(ra);
    throw std::runtime_error("Resource-B allocation error");
}

ResourceC *rc = allocate_resource_c();
if(rc == nullptr){
    free_resource_a(ra);
    free_resource_b(rb);
    throw std::runtime_error("Resource-C allocation error");
}

...

```
The source code will become messy and hard to maintain if the resource allocation logic is complex and there are many steps to complete, the allocate_xxx() / free_xxx() api callings must be matched to avoid resource leakage. 


The ON_EXIT() will greatly improve the code readibility, because whenever a resource is being allocated in the code, its lifetime control logic can be implemented inside a ON_EXIT() block nearby, defining the scope of resource and the way to release the resource, which make the code much more easy to maintain without unexpected resource leakage.  

```c++
{ //Begin ResourceA Scope
    ResourceA* ra = allocate_resource_a();
    ON_EXIT(
        ... //resource free logic
        if(must_release_resource_a()){
            free_resource_a(ra);
        }
    );
    ... //use Resource-A
    //
    { //Begin Resource-B Scope
        ResourceB* rb = allocate_resource_b();
        ON_EXIT(
            ... //resource free logic
            if(must_release_resource_b()){
                free_resource_b(rb);
            }
        );

        //Both resource-A and resource-B are available
        ...

    } //End Resource-B Scope
} //End of Resource A Scope

```

References:


[C.41: A constructor should create a fully initialized object](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c41-a-constructor-should-create-a-fully-initialized-object);
[R.1: Manage resources automatically using resource handles and RAII (Resource Acquisition Is Initialization)](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r1-manage-resources-automatically-using-resource-handles-and-raii-resource-acquisition-is-initialization);

### INTERFACE PROGRAMMING


In xputil, we have implemented a subsystem for interface define, implementation, cluster and discovery. 

First let's review what the interface based programming looks like, and why the interface programming can help improve the code partition of a middle to large c++ project.  

#### Interface-based Design
Interface-based programming is a very important infrastructure in OOP languages. An interface specifies a small set of behavior that an object should expose to the public; A class can implement
multiple independent interfaces. 

In _Go_, you can specify an interface via a built-in keyword _interface_:

```go
type IBark interface {
    bark()
}
type IRun interface {
    run()
}

type Dog struct {
}

func (inst* Dog) bark() {
    fmt.Println("dog barks!")
}
func (inst* Dog) run() {
    fmt.Println("dog can run")
}
``` 

In _Object Pascal_ (Delphi, Free-Pascal), the __Interface__ is also a built-in element:

```pascal
Type

IBark = Interface
    procedure bark;
end;

IRun = Interface
    procedure run;
end;

Dog = class(TInterfacedObject, IBark, IRun)
  procedure bark;  
  procedure run;  
end;

Implementation

procedure Dog.bark;
begin
   writeln("dog barks!");
end;

procedure Dog.run;
begin
   writeln("dog can run.");
end;
```

In C++, there is no such a built-in __interface__ keyword (MSVC does have non-standard ["__interface"](https://docs.microsoft.com/en-us/cpp/cpp/interface?redirectedfrom=MSDN&view=msvc-170) keyword for COM-related development) but its functionality can be implemented using pure abstract class.

```c++
struct IBark {
  virtual void bark() = 0;
};
struct IRun {
  virtual void run() = 0;
};

class Dog: public IBark, public IRun {
public:
    void bark() overload {
        printf("dog barks!\n");
    }
    void run() overload {
        printf("dog can run.\n");
    }
};
```

In the C++ guide line, there is a section talking about the interface based design:
[C.129: When designing a class hierarchy, distinguish between implementation inheritance and interface inheritance](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c129-when-designing-a-class-hierarchy-distinguish-between-implementation-inheritance-and-interface-inheritance)

The basic advantage of interface programming is __API abstraction__: the API and its implementation is 
completely isolated, as a result a software component implementing a well-known interface can be replaced or upgraded without breaking all of the exiting client codes.

However, with modern C++ template programming becomes more popular and powerful, An interface can also be implemented as a _contract_ or _concept_, which makes a class composible, reusable with minimum or flatten class hierarchy depth, the performance can also be improved by eliminating the virtual function overloading process at runtime.


Now that we know interface programming is very helpful, let's go a step further to talk about what kind of features are possibly needed to support the interface programming, and how XPUTIL can provide a simple solution for each problem.

#### Interface Definition and Implementation

For a simple c++ project where all source files are compiled into a single library or executable, interface can be implemented just like the Dog::IBark, Dog::IRun demo code, the compiler can see all of the interface implementations and all of the c++ object instances are managed by the same runtime context (memory pool, process heap, etc.).

![Single Module](./doc/single-mod.svg)

The life time control of interface objects becomes complicated for a middle to large size of project which have many software submodules, binary plugins. The interface-based programming can be a great fit in this scenerio to decouple the implementation of components via a set of well known interfaces.

However, in the multiple modules software, the API design must be very careful to avoid unexpected behaviors, such as object lifetime control across module boundary.

![Multiple Module Object: Bad](./doc/multi-mod.svg)

The resource is allocated in module-A but try to be released in module-B, which is unsafe because a c++ object can be managed in a customized memory pool or heap, the two software modules are compiled binaries that might not share the same process heap. In some OS different threads might have different heap, In Windows you can also allocate object in different heaps, since Module-B has no control of the runtime context of module-A, the across-boundary c++ object release does not make sense and will most likely cause unexpected behavior!

To fix the issue, we must make sure a c++ object must be managed by the same module (runtime context), so at the API layer both object create and object release functions must be provided for public access by external modules:

![Multiple Module Object: Safe](./doc/multi-mod-safe.svg)



Based on this observation the destructor of c++ class implementing an interface should be protected or private to avoid direct object deletion in the local runtime context.

On the other hand, a c++ interface object returned from a module might be shared by many clients, so the direct api calling to destroy an interface object is also not safe, its lifetime management must be implemented in a consistent way, that's why _XPUTIL_ uses a reference count based scheme.


In Module-A, the interface is declared and published in its api header file "intf_bark.h":

```c++
// module_a/intf_bark.h

#include <xputil/intf_defs.h>

struct IBark: public xp::IRefObj {
    virtual void bark() = 0;
};

IBark * create_bark();
```

The implementation unit in module-A looks like:

```c++
// module_a/impl_bark.cpp:
#include <xputil/impl_intfs.h>
#include "intf_bark.h"

IBark* create_bark(){
    return xp::make_ref<IBark>();
}

```


On the client side (module-B), we can call the module-a's api for an IBark interface object:

```c++
#include <module_a/intf_bark.h>

void func() {
    xp::auto_ref pr = create_bark();
    pr->bark();
}
```

The __xp::auto_ref<T>__ helper class is a RAII wrapper to increate / decrease automatically the reference count of an interface object, the lifetime of an interface object is fully managed in the runtime context of its source module (module-a), the potential resource leakage and cross-boundary c++ object destruction issue are resolved gracefully. 


#### Interface Publish and Discovery





#### Interchangablity