# guess.em - A number-guessing game (interactive: uses getinput).

int secret = 7;
print("I'm thinking of a number between 1 and 10.");

int tries = 0;
while (true) {
    print("Your guess?");
    int guess = int(getinput());
    tries = tries + 1;
    if (guess == secret) {
        print("Got it in {tries} tries!");
        break;
    }
    elif (guess < secret) { print("Higher..."); }
    else { print("Lower..."); }
}
