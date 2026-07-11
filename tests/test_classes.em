# class definition (spec example shape)
class Engine(cyl) {
    int cylinders = cyl;
    fn describe() { return "engine with {cylinders} cylinders"; }
}

class Vehicle(vname, ecyl) {
    string name = vname;
    int a = 2;
    int b;
    char mychar = 'j';
    Engine motor(ecyl);
    fn label() { return "{name} ({motor.cylinders} cyl)"; }
}

Vehicle v("Rig", 8);
print(v.name);
print(v.a, " ", v.b, " ", v.mychar);
print(v.motor.cylinders);
print(v.motor.describe());
print(v.label());

# field assignment
v.name = "Renamed";
print(v.label());
v.motor.cylinders = 12;
print(v.motor.describe());

# methods mutating fields
class Counter(start) {
    int count = start;
    fn bump() { count = count + 1; }
    fn current() { return count; }
}
Counter ctr(5);
ctr.bump();
ctr.bump();
print(ctr.current());

# inheritance (spec example shape)
class Animal(aname, sound) {
    string label = aname;
    string noise = sound;
    fn speak() { return "{label} says {noise}"; }
}
class Puppy(pname) = Animal(pname, "yip") {
    int age = 1;
}
Puppy pup("Spot");
print(pup.speak());
print(pup.age);
print(pup.label);

# object assignment without params + pass-by-value copies
class Car(speed) { int velocity = speed; }
Car oldcar(88);
class newcar;
newcar = oldcar;
print(newcar.velocity);
oldcar.velocity = 11;
print(newcar.velocity);

# class reassignment with automatic typecast
class Motorcycle(speed) { int velocity = speed; }
Motorcycle bike(100);
print(bike.velocity);
bike = Car(200);
print(bike.velocity);
bike = Car;
print(bike.velocity);

# nested classes
class Outer(a) {
    int val = a;
    class Inner(b) { int innerVal = b; }
}
Outer outer(10);
print(outer.val);

# equality on objects
Car c1(50);
Car c2(50);
Car c3(60);
print(c1 == c2, " ", c1 == c3, " ", c1 === c1);

# printing an object shows its fields (methods hidden)
print(ctr);

# regression: methods on copies act on the copy (including inherited methods)
class Base(v) {
    int shared = v;
    fn bump() { shared = shared + 1; }
    fn get() { return shared; }
}
class Derived(v) = Base(v) {
    int extra = 100;
    fn total() { return shared + extra; }
}
Derived d1(5);
class d2;
d2 = d1;
d1.bump();
d1.bump();
print(d1.get(), " ", d2.get());
print(d1.total(), " ", d2.total());
d2.bump();
print(d1.get(), " ", d2.get());
class Inner2(n) { int x = n; fn setx(v) { x = v; } fn getx() { return x; } }
class Outer2(n) { Inner2 core(n); }
Outer2 o1(10);
class o2;
o2 = o1;
o1.core.setx(77);
print(o1.core.getx(), " ", o2.core.getx());
