import testmod;
print(testmod.square(6));
print(testmod.magic);
print(testmod.greeting);

from testmod import cube;
print(cube(3));

from testmod import Point;
Point p(3, 4);
print(p.dist());

# imported module functions can use their own module's globals
print(testmod.scaledMagic(2));
