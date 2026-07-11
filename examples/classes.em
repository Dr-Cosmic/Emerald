# classes.em - Classes, inheritance, and Emerald's automatic typecasting.

class Animal(name, sound) {
    string label = name;
    string noise = sound;
    int legs = 4;

    fn speak() {
        print("{label} says {noise}");
    }
}

# Inheritance: a Puppy is an Animal whose sound is always "yip".
class Puppy(pname) = Animal(pname, "yip") {
    int age = 1;
    fn birthday() {
        age = age + 1;
        print("{label} is now {age}");
    }
}

Animal cat("Whiskers", "meow");
cat.speak();

Puppy pup("Spot");
pup.speak();
pup.birthday();
pup.birthday();

# Objects are copied by value.
class oldPup;
oldPup = pup;
pup.label = "Rex";
pup.speak();
oldPup.speak();   # still Spot - Emerald passes everything by value

# Class reassignment with automatic typecast (from the language spec):
class Motorcycle(speed) { int velocity = speed; }
class Car(speed) { int velocity = speed; int wheels = 4; }

Motorcycle mymotorcycle(120);
print("motorcycle velocity: {mymotorcycle.velocity}");
mymotorcycle = Car(90);   # mymotorcycle is now a car; old data is deleted
print("car velocity: {mymotorcycle.velocity}, wheels: {mymotorcycle.wheels}");
